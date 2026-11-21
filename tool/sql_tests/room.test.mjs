// Synthetic PostgreSQL tests, NOT a live Supabase migration or RLS/device QA.
// Production has additional triggers/tables/functions; see docs/PRO_RELEASE_GATE_2026-11-21.md.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';
import {PGlite} from '@electric-sql/pglite';

const migration = (name) => readFileSync(new URL(`../../supabase/games/migrations/${name}`, import.meta.url), 'utf8');
const host = '00000000-0000-0000-0000-000000000001';
const alice = '00000000-0000-0000-0000-000000000002';
const bob = '00000000-0000-0000-0000-000000000003';
const spectator = '00000000-0000-0000-0000-000000000004';
const newcomer = '00000000-0000-0000-0000-000000000005';
const room = '00000000-0000-0000-0000-000000000100';
const gameRoom = '00000000-0000-0000-0000-000000000101';
const match = '00000000-0000-0000-0000-000000000200';

async function setup(db) {
  await db.exec(`
    create role anon; create role authenticated; create role service_role;
    create schema auth;
    create function auth.uid() returns uuid language sql stable as $$
      select nullif(current_setting('app.test_uid', true), '')::uuid $$;
    create function auth.jwt() returns jsonb language sql stable as $$ select '{}'::jsonb $$;
    create table public.profiles(id uuid primary key, username text);
    create table public.nx_rooms(id uuid primary key, code text not null unique,
      game text not null, selected_game text, kind text not null default 'talk',
      status text not null default 'lobby', host uuid not null, title text,
      is_private boolean not null default false, speaker_capacity int, spectator_capacity int,
      seats int not null default 2, quick boolean not null default false, match_id uuid,
      updated_at timestamptz default now(), selected_seats int[] default '{}');
    create table public.nx_room_members(room_id uuid, user_id uuid, role text,
      last_seen timestamptz default now(), joined_at timestamptz default now(),
      primary key(room_id,user_id));
    create table public.nx_room_players(room_id uuid, user_id uuid, seat int, ready bool default false,
      last_seen timestamptz default now(), primary key(room_id,user_id), unique(room_id,seat));
    create table public.nx_room_game_players(room_id uuid, user_id uuid, seat int, team int,
      prefs jsonb not null default '{}', ready bool not null default false,
      primary key(room_id,user_id), unique(room_id,seat));
    create table public.nx_matches(id uuid primary key, room_id uuid, game text, players uuid[] not null,
      names text[] not null, status text default 'playing');
    create table public.nx_invites(id bigserial primary key, from_user uuid, to_user uuid, kind text,
      room_id uuid, payload jsonb, status text not null default 'pending',
      created_at timestamptz not null default now());
    create function public.nx_room_member_role(p_room uuid) returns text language sql stable security definer as $$
      select role from public.nx_room_members where room_id=p_room and user_id=auth.uid() $$;
    create function public.nx_in_room(p_room uuid) returns boolean language sql stable security definer as $$
      select exists(select 1 from public.nx_room_players where room_id=p_room and user_id=auth.uid()) $$;
    create function public.nx_is_room_member(p_room uuid) returns boolean language sql stable security definer as $$
      select exists(select 1 from public.nx_room_members where room_id=p_room and user_id=auth.uid()) $$;
    create function public._nx_leave_all(uid uuid) returns void language sql security definer as $$
      select null::void $$;
    grant usage on schema auth to authenticated;
    grant select on public.nx_rooms, public.nx_room_members to authenticated;
    alter table public.nx_rooms enable row level security;
    alter table public.nx_room_members enable row level security;
    create policy nx_room_members_read on public.nx_room_members for select to authenticated
      using (public.nx_is_room_member(room_id));
  `);
  await db.exec(migration('20261121110000_g_registered_game_seats.sql'));
  await db.exec(`create function public._nx_room_seat_limit(p_game text) returns integer
    language sql immutable as $$ select (public.nx_game_bounds(p_game)).max_seats $$;`);
  await db.exec(migration('20261121130000_g_room_admission_integrity.sql'));
  await db.exec(`
    insert into profiles values ('${host}', 'Host'), ('${alice}', 'Alice'),
      ('${bob}', 'Bob'), ('${spectator}', 'Spectator');
    insert into nx_rooms(id,code,game,selected_game,host,is_private,seats,title)
      values ('${room}','PRIV01','finger_soccer','finger_soccer','${host}',true,6,'Invitation Only');
    insert into nx_room_members(room_id,user_id,role) values ('${room}','${host}','manager');
  `);
}
const as = (db, uid) => db.exec(`set app.test_uid = '${uid}'`);
const value = async (db, sql) => (await db.query(sql)).rows[0];
const denied = async (db, sql, message) => {
  await assert.rejects(() => db.query(sql), message);
};

// Deliberately one test per DB to check the *sequence* of room transitions.
test('private admission, RLS, selection, moderation, late join and legacy route', async () => {
  const db = new PGlite();
  try {
    await setup(db);
    await as(db, spectator);
    await db.exec('set role authenticated');
    assert.equal((await value(db, `select count(*)::int n from nx_rooms where id='${room}'`)).n, 0);
    await db.exec('reset role');
    await denied(db, `select nx_room_join('${room}','speaker')`, 'a bare room id is not an invitation');
    await denied(db, `select nx_join_room(null,'${room}')`, 'the old lobby route cannot bypass talk admission');

    await as(db, host);
    await denied(db, `select nx_room_select_players('${room}', array['${host}'::uuid,'${host}'::uuid])`, 'duplicate seats');
    await db.query(`select nx_room_invite('${room}','${alice}')`);
    // Invite someone who has not yet visited games: their games-project profile is absent.
    await db.query(`select nx_room_invite('${room}','${newcomer}')`);
    assert.equal((await value(db, `select count(*)::int n from nx_invites where to_user='${newcomer}'`)).n, 1);
    await as(db, alice);
    await db.query(`select nx_room_join('${room}','speaker')`);
    await db.exec('set role authenticated');
    assert.equal((await value(db, `select count(*)::int n from nx_rooms where id='${room}'`)).n, 1,
      'an invited member must see their own private room');
    await db.exec('reset role');

    await as(db, host);
    await db.query(`select nx_room_select_players('${room}', array['${host}'::uuid,'${alice}'::uuid])`);
    await db.query(`update nx_room_game_players set prefs='{"team":"club0"}', ready=true
      where room_id='${room}' and user_id='${host}'`);
    await db.query(`update nx_room_game_players set prefs='{"team":"club1"}', ready=true
      where room_id='${room}' and user_id='${alice}'`);
    await db.query(`select nx_room_select_players('${room}', array['${alice}'::uuid,'${host}'::uuid])`);
    let selected = (await db.query(`select user_id::text id, seat, prefs, ready from nx_room_game_players
      where room_id='${room}' order by seat`)).rows;
    assert.deepEqual(selected.map((s) => [s.id, s.prefs.team, s.ready]),
      [[alice,'club1',true],[host,'club0',true]], 'swapping sides must preserve owned choices and readiness');
    await denied(db, `select nx_room_select_players('${room}', array['${host}'::uuid,'${bob}'::uuid])`, 'not a member');
    await db.query(`select nx_room_invite('${room}','${spectator}')`);
    await as(db, spectator);
    await db.query(`select nx_room_join('${room}','spectator')`);
    await as(db, host);
    await denied(db, `select nx_room_select_players('${room}', array['${host}'::uuid,'${spectator}'::uuid])`, 'listener cannot play');

    await db.query(`select nx_room_set_role('${room}','${alice}','spectator')`);
    selected = (await db.query(`select user_id::text id, seat, prefs from nx_room_game_players where room_id='${room}'`)).rows;
    assert.equal(selected.length, 1);
    assert.equal(selected[0].id, host);
    assert.equal(selected[0].seat, 0, 'demotion must compact gaps');
    await db.query(`select nx_room_kick('${room}','${spectator}')`);
    await as(db, spectator);
    await db.exec('set role authenticated');
    assert.equal((await value(db, `select count(*)::int n from nx_rooms where id='${room}'`)).n, 0,
      'kicked member must lose private room RLS access');
    assert.equal((await value(db, `select count(*)::int n from nx_room_members where room_id='${room}'`)).n, 0);
    await db.exec('reset role');
    await denied(db, `select nx_room_join('${room}','spectator')`, 'kicked member cannot rejoin');
    await as(db, host);
    await db.query(`select nx_room_unban('${room}','${spectator}')`);
    await as(db, spectator);
    await db.query(`select nx_room_join('${room}','spectator')`);

    // Mid-match: the invited late arrival is social-only; a manager cannot
    // demote/kick a player whose UUID is in the active nx_matches.players array.
    await as(db, host);
    await db.query(`select nx_room_set_role('${room}','${alice}','speaker')`);
    await db.query(`select nx_room_select_players('${room}', array['${host}'::uuid,'${alice}'::uuid])`);
    await db.query(`insert into nx_matches(id,room_id,game,players,names) values
      ('${match}','${room}','finger_soccer',array['${host}'::uuid,'${alice}'::uuid],array['Host','Alice'])`);
    await db.query(`update nx_rooms set status='playing',match_id='${match}' where id='${room}'`);
    await as(db, newcomer);
    await db.query(`select nx_room_join('${room}','speaker')`);
    assert.equal((await value(db, `select count(*)::int n from nx_room_game_players where user_id='${newcomer}'`)).n, 0);
    await as(db, host);
    await denied(db, `select nx_room_set_role('${room}','${alice}','spectator')`, 'active player demotion');
    await denied(db, `select nx_room_kick('${room}','${alice}')`, 'active player kick');
    await db.query(`update nx_matches set status='finished' where id='${match}'`);
    await db.query(`update nx_rooms set status='lobby' where id='${room}'`);
    await db.query(`select nx_room_kick('${room}','${alice}')`);
    await as(db, alice);
    await denied(db, `select nx_room_join('${room}','speaker')`, 'postmatch ban');

    // The old game-lobby route remains available with a private room CODE,
    // never through a leaked UUID or through a talk room.
    await as(db, host);
    await db.exec(`insert into nx_rooms(id,code,game,host,kind,is_private,seats)
      values ('${gameRoom}','GCODE1','carrom','${host}','game',true,2);
      insert into nx_room_players(room_id,user_id,seat) values ('${gameRoom}','${host}',0);`);
    await as(db, bob);
    await denied(db, `select nx_join_room(null,'${gameRoom}')`, 'game UUID without invite');
    await db.query(`select nx_join_room('GCODE1',null)`);
    assert.equal((await value(db, `select count(*)::int n from nx_room_players where room_id='${gameRoom}'`)).n, 2);
    await denied(db, `select nx_room_join('${gameRoom}','speaker')`, 'talk route cannot join game lobby');
  } finally {
    await db.close();
  }
});

test('match trigger enforces registered game-specific active seats, no duplicate players', async () => {
  const db = new PGlite();
  try {
    await setup(db);
    await denied(db, `insert into nx_matches(id,game,players,names) values
      ('${match}','chess_blitz',array['${host}'::uuid,'${alice}'::uuid],array['H','A'])`, 'metadata-only game');
    await denied(db, `insert into nx_matches(id,game,players,names) values
      ('${match}','quiz',array['${host}'::uuid,'${alice}'::uuid,'${bob}'::uuid],array['H','A','B'])`, 'quiz is 2 seats');
    await denied(db, `insert into nx_matches(id,game,players,names) values
      ('${match}','finger_soccer',array['${host}'::uuid,'${alice}'::uuid,'${bob}'::uuid],array['H','A','B'])`, 'balanced soccer sides');
    await denied(db, `insert into nx_matches(id,game,players,names) values
      ('${match}','carrom',array['${host}'::uuid,'${host}'::uuid],array['H','H'])`, 'duplicate account');
    await db.query(`insert into nx_matches(id,game,players,names) values
      ('${match}','finger_soccer',array['${host}'::uuid,'${alice}'::uuid,'${bob}'::uuid,'${spectator}'::uuid],array['H','A','B','S'])`);
    assert.equal((await value(db, `select cardinality(players) n from nx_matches where id='${match}'`)).n, 4);
  } finally {
    await db.close();
  }
});
