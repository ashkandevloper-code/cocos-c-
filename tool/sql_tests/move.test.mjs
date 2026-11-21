// Synthetic PostgreSQL checks for the games-project move/result migration.
// The minimal _nx_apply_result below records calls only; it does NOT simulate
// the real wallet bridge, anti-cheat validation or server rules of 16 games.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';
import {PGlite} from '@electric-sql/pglite';

const migration = readFileSync(new URL('../../supabase/games/migrations/20261121140000_g_match_move_integrity.sql', import.meta.url), 'utf8');
const u = [1, 2, 3, 4, 5].map((i) => `00000000-0000-0000-0000-00000000000${i}`);
const id = (i) => `00000000-0000-0000-0000-0000000002${String(i).padStart(2, '0')}`;

async function setup(db) {
  await db.exec(`
    create role anon; create role authenticated; create role service_role;
    create schema auth;
    create function auth.uid() returns uuid language sql stable as $$
      select nullif(current_setting('app.test_uid', true),'')::uuid $$;
    create table public.profiles(id uuid primary key, xp int default 0);
    create table public.nx_rooms(id uuid primary key, status text default 'playing', match_id uuid,
      updated_at timestamptz default now());
    create table public.nx_room_players(room_id uuid, user_id uuid, seat int,
      ready bool default false, last_seen timestamptz default now());
    create table public.nx_matches(id uuid primary key, room_id uuid,
      game text not null, players uuid[] not null, names text[] default '{}',
      settings jsonb not null default '{}', status text default 'playing', winner_seats int[],
      scores int[], reason text, last_seq int not null default 0, state_hash text,
      turn_seat int not null default 0, turn_deadline timestamptz not null default (now() + interval '45 seconds'),
      started_at timestamptz not null default now(), ended_at timestamptz);
    create table public.nx_match_moves(match_id uuid, seq int, seat int, by_user uuid,
      kind text not null, move jsonb not null, hash text,
      created_at timestamptz not null default now(), primary key(match_id, seq));
    create table public.nx_match_reports(match_id uuid, user_id uuid, winners int[] not null,
      scores int[], state_hash text, reason text, created_at timestamptz default now(),
      primary key(match_id, user_id));
    create table public.nx_result_awards(match_id uuid primary key, winners int[], scores int[]);
    create function public.nx_in_match(p_match uuid) returns boolean language sql stable security definer as $$
      select exists(select 1 from public.nx_matches where id = p_match and auth.uid() = any(players)) $$;
    create function public.nx_has_perm(p text, uid uuid default auth.uid()) returns boolean language sql stable as $$
      select false $$;
    create function public._nx_apply_result(m public.nx_matches, winners int[], scores int[])
      returns void language plpgsql security definer as $$
      begin
        insert into public.nx_result_awards(match_id, winners, scores) values (m.id,winners,scores)
          on conflict (match_id) do nothing;
      end $$;
    create function public.nx_finish_match_as(p_match uuid, p_winners int[], p_loser int)
      returns jsonb language plpgsql security definer as $$
      declare m public.nx_matches;
      begin
        update public.nx_matches set status='finished',winner_seats=p_winners,
          reason='forfeit:' || p_loser, ended_at=now()
          where id=p_match and status='playing' returning * into m;
        if m.id is null then return jsonb_build_object('ok',false); end if;
        perform public._nx_apply_result(m,p_winners,null);
        update public.nx_rooms set status='lobby' where id=m.room_id and match_id=m.id;
        return jsonb_build_object('ok',true,'winners',p_winners,'forfeit',p_loser);
      end $$;
    grant usage on schema auth to authenticated;
    grant select on public.nx_matches to authenticated;
    alter table public.nx_matches enable row level security;
  `);
  await db.exec(migration);
  await db.exec(`insert into profiles(id) values ('${u[0]}'),('${u[1]}'),('${u[2]}'),('${u[3]}'),('${u[4]}');`);
}
const as = (db, who) => db.exec(`set app.test_uid='${who}'`);
const value = async (db, sql, params) => (await db.query(sql, params)).rows[0];
const denied = async (db, sql, params) => assert.rejects(() => db.query(sql, params));
const createMatch = (db, mid, game, seats = 2, settings = {}) => db.query(
  `insert into nx_matches(id,game,players,names,settings)
    values ($1::uuid,$2::text,$3::uuid[],$4::text[],$5::jsonb)`,
  [mid, game, `{${u.slice(0,seats).join(',')}}`, `{${['A','B','C','D'].slice(0,seats).join(',')}}`, JSON.stringify(settings)],
);
const push = async (db, mid, seq, kind, data, seat) => (await value(db,
  'select nx_push_move($1::uuid,$2::integer,$3::text,$4::jsonb,null::text,$5::integer) v',
  [mid,seq,kind,JSON.stringify(data),seat])).v;
const finish = async (db, mid, winners, scores, hash, reason = 'finished') => (await value(db,
  'select nx_finish_match($1::uuid,$2::integer[],$3::integer[],$4::text,$5::text) v',
  [mid,`{${winners.join(',')}}`,scores == null ? null : `{${scores.join(',')}}`,hash,reason])).v;
const forfeit = async (db, mid, seat = null) => (await value(db,
  'select nx_forfeit($1::uuid,$2::integer) v', [mid,seat])).v;

test('match RLS, trusted heartbeats, cross-seat restrictions and two-player forfeit', async () => {
  const db = new PGlite();
  try {
    await setup(db);
    const mid = id(1);
    await createMatch(db, mid, 'quiz');
    await as(db,u[4]);
    await db.exec('set role authenticated');
    assert.equal((await value(db, `select count(*)::int n from nx_matches where id='${mid}'`)).n,0);
    await db.exec('reset role');
    await denied(db, 'select nx_match_ping($1::uuid)', [mid]);
    await as(db,u[0]);
    await db.query('select nx_match_ping($1::uuid)', [mid]);
    await as(db,u[1]);
    await db.query('select nx_match_ping($1::uuid)', [mid]);
    await db.exec('set role authenticated');
    assert.equal((await value(db, `select count(*)::int n from nx_matches where id='${mid}'`)).n,1);
    await db.exec('reset role');
    await denied(db, 'select nx_push_move($1::uuid,1,$2::text,$3::jsonb,null::text,0)',
      [mid,'system',JSON.stringify({_k:'move',a:'ans',c:1})]);
    await denied(db, 'select nx_push_move($1::uuid,1,$2::text,$3::jsonb,null::text,0)',
      [mid,'snap',JSON.stringify({a:'snap',m:{}})]);
    await denied(db, 'select nx_push_move($1::uuid,1,$2::text,$3::jsonb,null::text,0)',
      [mid,'system',JSON.stringify({_k:'timeout',a:'ans',c:-1,r:0,ms:15000})]);
    await as(db,u[0]);
    assert.equal((await push(db,mid,1,'system',{_k:'timeout',a:'shot'},1)).reason,'bad_timeout');
    assert.equal((await push(db,mid,1,'system',{_k:'timeout',a:'ans',c:-1,r:0,ms:15000},1)).ok,true);
    await denied(db, 'select nx_push_move($1::uuid,2,$2::text,$3::jsonb,null::text,1)',
      [mid,'forfeit',JSON.stringify({a:'leave',seat:1})]);
    await as(db,u[1]);
    assert.equal((await push(db,mid,2,'system',{_k:'move',a:'ans',r:0,c:1},1)).ok,true);
    await denied(db, 'select nx_push_move($1::uuid,3,$2::text,$3::jsonb,null::text,1)',
      [mid,'forfeit',JSON.stringify({a:'leave',seat:0})]);
    const left = await forfeit(db,mid);
    assert.deepEqual(left.winners,[0]);
    assert.equal((await forfeit(db,mid)).already,true, 'retries do not log a second forfeit');
    assert.equal((await value(db, `select count(*)::int n from nx_result_awards where match_id='${mid}'`)).n,1);
    assert.equal((await value(db, `select count(*)::int n from nx_match_moves where match_id='${mid}' and kind='forfeit'`)).n,1);
    await denied(db, 'select nx_match_ping($1::uuid)', [mid]);

    // Older clients that write a self-forfeit to the move log also settle.
    const old = id(2);
    await createMatch(db,old,'carrom');
    await as(db,u[0]);
    const logged = await push(db,old,1,'forfeit',{a:'leave',seat:0},0);
    assert.equal(logged.ok,true);
    assert.deepEqual((await value(db, `select winner_seats winners from nx_matches where id='${old}'`)).winners,[1]);
    assert.equal((await value(db, `select count(*)::int n from nx_result_awards where match_id='${old}'`)).n,1);
  } finally { await db.close(); }
});

test('survivor-only consensus, score validation, disputes and reconnect grace', async () => {
  const db = new PGlite();
  try {
    await setup(db);
    const mid = id(3);
    await createMatch(db,mid,'finger_soccer',4);
    await as(db,u[2]);
    assert.equal((await finish(db,mid,[2],[0,0,8,0],'old')).pending,true);
    assert.equal((await forfeit(db,mid)).ok,true);
    assert.equal((await finish(db,mid,[2],[0,0,8,0],'old')).error,'forfeited_reporter');
    await as(db,u[0]);
    assert.equal((await finish(db,mid,[0,1,3],[6,5,0,2],'agree')).pending,true);
    assert.equal((await finish(db,mid,[0,1,3],[6,5],'agree')).error,'bad_scores',
      'a malformed retry cannot replace the first claim');
    await as(db,u[1]);
    assert.equal((await finish(db,mid,[0,1,3],[4,7,0,3],'agree')).pending,true);
    await as(db,u[3]);
    assert.equal((await finish(db,mid,[0,1,3],[5,6,0,1],'agree')).ok,true);
    const result = await value(db, `select status,winner_seats winners,scores from nx_matches where id='${mid}'`);
    assert.equal(result.status,'finished');
    assert.deepEqual(result.winners,[0,1,3]);
    assert.deepEqual(result.scores,[4,5,0,1], 'the forfeiter\'s earlier score is ignored');
    assert.equal((await value(db, `select count(*)::int n from nx_result_awards where match_id='${mid}'`)).n,1);

    const bad = id(4);
    await createMatch(db,bad,'quiz');
    await as(db,u[0]);
    assert.equal((await finish(db,bad,[0],[9],'x')).error,'bad_scores');
    assert.equal((await finish(db,bad,[0],[9,2],'x')).pending,true);
    await as(db,u[1]);
    assert.equal((await finish(db,bad,[0],[9,2],'y')).disputed,true);
    assert.equal((await value(db, `select status from nx_matches where id='${bad}'`)).status,'disputed');
    assert.equal((await value(db, `select count(*)::int n from nx_result_awards where match_id='${bad}'`)).n,0);

    const grace = id(5);
    await createMatch(db,grace,'quiz',2,{grace_sec:90});
    await db.query(`update nx_matches set started_at=now()-interval '4 minutes',
      turn_deadline=now()-interval '1 minute' where id='${grace}'`);
    await db.query(`update nx_match_heartbeats set seen_at=now()-interval '50 seconds'
      where match_id='${grace}' and user_id='${u[1]}'`);
    // The heartbeat is for this match, not a different one.
    await db.query(`insert into nx_match_heartbeats(match_id,user_id,seen_at)
      values ('${grace}','${u[1]}',now()-interval '50 seconds')`);
    await as(db,u[0]);
    assert.equal((await finish(db,grace,[0],[2,1],null)).pending,true);
    await db.query(`update nx_match_heartbeats set seen_at=now()-interval '2 minutes'
      where match_id='${grace}' and user_id='${u[1]}'`);
    assert.equal((await finish(db,grace,[0],[2,1],null)).ok,true);
    assert.equal((await value(db, `select status from nx_matches where id='${grace}'`)).status,'finished');
  } finally { await db.close(); }
});

test('host cannot forfeit a live opponent; authority fails over; every leaver abandons a team match', async () => {
  const db = new PGlite();
  try {
    await setup(db);
    const early = id(6);
    await createMatch(db,early,'quiz');
    await as(db,u[0]);
    assert.equal((await forfeit(db,early,1)).reason,'active_or_reconnecting');
    await db.query(`update nx_matches set started_at=now()-interval '3 minutes' where id='${early}'`);
    await as(db,u[1]);
    await db.query(`select nx_match_ping('${early}')`);
    await as(db,u[0]);
    assert.equal((await forfeit(db,early,1)).reason,'active_or_reconnecting',
      'the target is connected but may be idle');
    await db.query(`update nx_match_heartbeats set seen_at=now()-interval '3 minutes' where match_id='${early}'`);
    assert.equal((await forfeit(db,early,1)).ok,true);

    const failover = id(7);
    await createMatch(db,failover,'quiz');
    await db.query(`update nx_matches set started_at=now()-interval '3 minutes' where id='${failover}'`);
    await as(db,u[1]);
    await db.query(`select nx_match_ping('${failover}')`);
    const takeover = await forfeit(db,failover,0);
    assert.deepEqual(takeover.winners,[1], 'seat 1 is authority after seat 0 is absent beyond grace');

    const team = id(8);
    await createMatch(db,team,'finger_soccer',4);
    for (let s=0; s<4; s++) {
      await as(db,u[s]);
      assert.equal((await forfeit(db,team)).ok,true);
    }
    assert.equal((await value(db, `select status from nx_matches where id='${team}'`)).status,'abandoned');
    assert.equal((await value(db, `select count(*)::int n from nx_result_awards where match_id='${team}'`)).n,0,
      'unopposed abandonment is never a rewarded draw');
  } finally { await db.close(); }
});
