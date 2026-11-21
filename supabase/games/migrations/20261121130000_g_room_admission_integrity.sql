-- NEXORA PRO · room membership is NOT a game seat. Apply to GAMES project
-- after 20261121110000_g_registered_game_seats and the custom-team guard.
-- Forward-only: preserve existing rooms/members. No universal audience cap;
-- the per-game bounds apply only to selected active match players.

-- A manager's removal must survive a public-room rejoin. These bans are
-- room-local, not account-wide, and are never exposed by a table RLS policy.
create table if not exists public.nx_room_bans (
  room_id uuid not null references public.nx_rooms(id) on delete cascade,
  user_id uuid not null references public.profiles(id) on delete cascade,
  by_user uuid not null references public.profiles(id),
  created_at timestamptz not null default now(),
  primary key (room_id, user_id)
);
alter table public.nx_room_bans enable row level security;
revoke all on table public.nx_room_bans from public, anon, authenticated;
grant all on table public.nx_room_bans to service_role;

-- Protect match creation even when an older RPC calls INSERT: metadata-only
-- games, duplicate accounts, odd soccer sides and over-capacity teams are
-- rejected by the database. Existing historical matches are not rewritten.
create or replace function public._nx_validate_active_match() returns trigger
language plpgsql security definer set search_path = public as $$
declare lo int; hi int; n int;
begin
  if not public.nx_known_game(new.game) then raise exception 'unknown game'; end if;
  select min_seats, max_seats into lo, hi from public.nx_game_bounds(new.game);
  n := cardinality(new.players);
  if lo is null or hi is null or n < lo or n > hi then
    raise exception 'invalid active seats for %: % (allowed %..%)', new.game, n, lo, hi;
  end if;
  if new.game = 'finger_soccer' and n % 2 <> 0 then
    raise exception 'finger soccer needs balanced teams';
  end if;
  if array_position(new.players, null) is not null or
     n <> (select count(distinct u) from unnest(new.players) u) then
    raise exception 'duplicate or empty match player';
  end if;
  if cardinality(new.names) <> n then raise exception 'match names do not match seats'; end if;
  return new;
end $$;
revoke all on function public._nx_validate_active_match() from public, anon, authenticated;
drop trigger if exists nx_validate_active_match on public.nx_matches;
create trigger nx_validate_active_match before insert or update of game, players, names
on public.nx_matches for each row execute function public._nx_validate_active_match();

-- Only a manager/speaker who is an actual room member may be selected for a
-- seat; never auto-promote a spectator to an active player by crafting an RPC.
create or replace function public.nx_room_select_players(p_room uuid, p_users uuid[])
returns jsonb language plpgsql security definer set search_path = public as $$
declare i int := 0; uid uuid; r public.nx_rooms; lim int; users uuid[] := coalesce(p_users, '{}'::uuid[]);
        previous jsonb; saved jsonb;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null then raise exception 'room not found'; end if;
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  if r.kind <> 'talk' or r.status <> 'lobby' then raise exception 'room is not a waiting talk room'; end if;
  lim := (public.nx_game_bounds(coalesce(r.selected_game, r.game))).max_seats;
  if lim is null or not public.nx_known_game(coalesce(r.selected_game, r.game)) then raise exception 'unknown game'; end if;
  if cardinality(users) > lim then raise exception 'game supports at most % active players', lim; end if;
  if cardinality(users) <> (select count(distinct u) from unnest(users) u) then
    raise exception 'duplicate or empty selected player';
  end if;
  if exists(select 1 from unnest(users) u where not exists(
    select 1 from public.nx_room_members m where m.room_id = p_room and m.user_id = u
      and m.role in ('manager', 'speaker'))) then
    raise exception 'selected players must be speakers in this room';
  end if;
  -- The order can change (notably Swap Teams in Finger Soccer). Capture each
  -- user's own prefs/readiness BEFORE re-numbering: deleting then inserting
  -- with defaults used to silently erase paid team choices and ready state.
  select coalesce(jsonb_object_agg(user_id::text,
    jsonb_build_object('prefs', prefs, 'ready', ready)), '{}'::jsonb)
    into previous from public.nx_room_game_players where room_id = p_room;
  delete from public.nx_room_game_players where room_id = p_room;
  foreach uid in array users loop
    saved := previous -> uid::text;
    insert into public.nx_room_game_players(room_id, user_id, seat, team, prefs, ready)
    values (p_room, uid, i, i % 2,
      coalesce(saved -> 'prefs', '{}'::jsonb),
      coalesce((saved ->> 'ready')::boolean, false));
    i := i + 1;
  end loop;
  update public.nx_rooms set selected_seats = coalesce((select array_agg(seat order by seat)
    from public.nx_room_game_players where room_id = p_room), '{}'::integer[]), updated_at = now()
  where id = p_room;
  return jsonb_build_object('selected', i);
end $$;

-- Joins serialize on the room row; active seats are only allocated in the
-- waiting lobby. People joining a running game are spectators, not new players.
-- Private talk rooms require an unexpired invitation (a leaked UUID is not one).
create or replace function public.nx_room_join(p_room uuid, p_role text default 'speaker')
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); r public.nx_rooms; role_to_use text;
        speakers int; spectators int;
begin
  if me is null then raise exception 'not signed in'; end if;
  if p_role is null or p_role not in ('speaker', 'spectator') then raise exception 'invalid room role'; end if;
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null then raise exception 'room not found'; end if;
  if r.kind <> 'talk' then raise exception 'use the game-lobby admission route'; end if;
  if r.status = 'closed' then raise exception 'room closed'; end if;
  if exists (select 1 from public.nx_room_bans b where b.room_id = p_room and b.user_id = me) then
    raise exception 'removed from this room';
  end if;
  if exists (select 1 from public.nx_room_members m where m.room_id = p_room and m.user_id = me) then
    update public.nx_room_members set last_seen = now() where room_id = p_room and user_id = me;
    return jsonb_build_object('room_id', p_room, 'role', public.nx_room_member_role(p_room),
      'rejoined', true, 'status', r.status, 'match_id', r.match_id);
  end if;
  if r.kind = 'talk' and r.is_private and me <> r.host and not exists (
    select 1 from public.nx_invites i where i.room_id = p_room and i.to_user = me
      and i.kind = 'talk' and i.status in ('pending','accepted')
      and i.created_at > now() - interval '7 days'
  ) then raise exception 'invitation required'; end if;
  insert into public.profiles(id, username)
  values (me, coalesce(auth.jwt() -> 'user_metadata' ->> 'username', 'Pilot'))
  on conflict (id) do nothing;
  select count(*) filter (where role in ('manager','speaker')),
         count(*) filter (where role = 'spectator') into speakers, spectators
    from public.nx_room_members where room_id = p_room;
  if p_role = 'spectator' then
    if r.spectator_capacity is not null and spectators >= r.spectator_capacity then raise exception 'room full'; end if;
    role_to_use := 'spectator';
  elsif r.speaker_capacity is null or speakers < r.speaker_capacity then role_to_use := 'speaker';
  elsif r.spectator_capacity is null or spectators < r.spectator_capacity then role_to_use := 'spectator';
  else raise exception 'room full'; end if;
  if role_to_use = 'spectator' and r.spectator_capacity is not null and spectators >= r.spectator_capacity then
    raise exception 'room full';
  end if;
  insert into public.nx_room_members(room_id, user_id, role) values (p_room, me, role_to_use);
  -- A talk-room join never allocates an active game seat, even mid-match.
  update public.nx_rooms set updated_at = now() where id = p_room;
  return jsonb_build_object('room_id', p_room, 'role', role_to_use, 'seat', null,
    'status', r.status, 'match_id', r.match_id);
end $$;

-- The legacy game-lobby RPC must not let a guessed private talk-room UUID
-- create an nx_room_players row (that would bypass the invite check above and
-- the older nx_rooms RLS helper). Game lobbies still accept their published
-- room id or a private room's code / recent game invitation.
create or replace function public.nx_join_room(p_code text default null, p_room uuid default null)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); r public.nx_rooms; s int;
begin
  if me is null then raise exception 'not signed in'; end if;
  if p_room is not null then select * into r from public.nx_rooms where id = p_room for update;
  else select * into r from public.nx_rooms where code = upper(btrim(p_code)) for update; end if;
  if r.id is null then raise exception 'room not found'; end if;
  if r.kind <> 'game' then raise exception 'use the talk-room admission route'; end if;
  if r.status = 'closed' then raise exception 'room closed'; end if;
  if exists(select 1 from public.nx_room_players where room_id = r.id and user_id = me) then
    update public.nx_room_players set last_seen = now() where room_id = r.id and user_id = me;
    return jsonb_build_object('room_id', r.id, 'code', r.code, 'rejoined', true,
      'status', r.status, 'match_id', r.match_id);
  end if;
  if r.status <> 'lobby' then raise exception 'room already started'; end if;
  if r.is_private and me <> r.host and
      (p_code is null or upper(btrim(p_code)) <> r.code) and not exists (
        select 1 from public.nx_invites i where i.room_id = r.id and i.to_user = me
          and i.kind = 'game' and i.status in ('pending','accepted')
          and i.created_at > now() - interval '7 days'
      ) then raise exception 'room code or invitation required'; end if;
  perform public._nx_leave_all(me);
  select min(g) into s from generate_series(0, r.seats - 1) g
    where not exists(select 1 from public.nx_room_players rp where rp.room_id = r.id and rp.seat = g);
  if s is null then raise exception 'room full'; end if;
  insert into public.nx_room_players(room_id, user_id, seat) values (r.id, me, s);
  update public.nx_rooms set updated_at = now() where id = r.id;
  return jsonb_build_object('room_id', r.id, 'code', r.code, 'seat', s, 'status', r.status);
end $$;

-- The old policy only recognized legacy nx_room_players. A valid member of an
-- invite-only talk room could join through the RPC, then RLS hid the very room
-- they had joined. Include the SECURITY DEFINER membership helper, without
-- making any private room public.
drop policy if exists nx_rooms_read on public.nx_rooms;
create policy nx_rooms_read on public.nx_rooms for select to authenticated
  using (not is_private or host = auth.uid() or public.nx_in_room(id) or public.nx_is_room_member(id));

-- Re-number remaining seats without deleting their paid preferences/readiness.
-- Ascending updates always fill a vacant lower seat, so the unique(room,seat)
-- constraint remains valid. Alternating Finger Soccer sides stay balanced.
create or replace function public._nx_compact_room_seats(p_room uuid)
returns void language plpgsql security definer set search_path = public as $$
declare g record; next_seat int := 0;
begin
  for g in select user_id from public.nx_room_game_players
             where room_id = p_room order by seat loop
    update public.nx_room_game_players set seat = next_seat, team = next_seat % 2
      where room_id = p_room and user_id = g.user_id;
    next_seat := next_seat + 1;
  end loop;
  update public.nx_rooms set selected_seats = coalesce((select array_agg(seat order by seat)
    from public.nx_room_game_players where room_id = p_room), '{}'::integer[]), updated_at = now()
  where id = p_room;
end $$;
revoke all on function public._nx_compact_room_seats(uuid) from public, anon, authenticated;

-- Leaving a waiting room also closes any gap in the remaining side/seat
-- sequence while preserving each player's preferences. Running match seats
-- remain immutable until settlement; someone else may join as a spectator.
create or replace function public.nx_room_leave(p_room uuid)
returns void language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); was text; nxt uuid; r public.nx_rooms;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null then return; end if;
  select role into was from public.nx_room_members where room_id = p_room and user_id = me;
  if was is null then return; end if;
  if r.status = 'lobby' then
    delete from public.nx_room_game_players where room_id = p_room and user_id = me;
    perform public._nx_compact_room_seats(p_room);
  end if;
  delete from public.nx_room_players where room_id = p_room and user_id = me;
  delete from public.nx_room_members where room_id = p_room and user_id = me;
  if was = 'manager' then
    select user_id into nxt from public.nx_room_members where room_id = p_room order by joined_at limit 1;
    if nxt is not null then
      update public.nx_room_members set role = 'manager' where room_id = p_room and user_id = nxt;
      update public.nx_rooms set host = nxt where id = p_room;
    end if;
  end if;
  if r.kind = 'talk' and not exists(select 1 from public.nx_room_members where room_id = p_room) then
    update public.nx_rooms set status = 'closed', updated_at = now() where id = p_room;
  else
    update public.nx_rooms set updated_at = now() where id = p_room;
  end if;
end $$;

-- Promotion/demotion is serialized with joins and may not silently reclassify
-- an active match seat as a spectator while that match is still playing.
create or replace function public.nx_room_set_role(p_room uuid, p_user uuid, p_role text)
returns void language plpgsql security definer set search_path = public as $$
declare r public.nx_rooms; speakers int; spectators int;
begin
  if p_role is null or p_role not in ('speaker', 'spectator') then raise exception 'bad role'; end if;
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null or r.status = 'closed' then raise exception 'room closed'; end if;
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  if p_user = auth.uid() then raise exception 'cannot change manager role'; end if;
  if p_role = 'spectator' and r.status = 'playing' and exists (
    select 1 from public.nx_matches m where m.id = r.match_id and m.status = 'playing'
      and p_user = any(m.players)
  ) then raise exception 'cannot demote an active match player'; end if;
  select count(*) filter (where role in ('manager','speaker')),
         count(*) filter (where role = 'spectator') into speakers, spectators
    from public.nx_room_members where room_id = p_room and user_id <> p_user;
  if p_role = 'speaker' and r.speaker_capacity is not null and speakers >= r.speaker_capacity then raise exception 'speaker capacity reached'; end if;
  if p_role = 'spectator' and r.spectator_capacity is not null and spectators >= r.spectator_capacity then raise exception 'spectator capacity reached'; end if;
  update public.nx_room_members set role = p_role, last_seen = now()
    where room_id = p_room and user_id = p_user and role <> 'manager';
  if not found then raise exception 'member not found'; end if;
  if p_role = 'spectator' and r.status = 'lobby' then
    delete from public.nx_room_game_players where room_id = p_room and user_id = p_user;
    perform public._nx_compact_room_seats(p_room);
  end if;
  update public.nx_rooms set updated_at = now() where id = p_room;
end $$;

create or replace function public.nx_room_set_access(p_room uuid, p_private boolean)
returns void language plpgsql security definer set search_path = public as $$
declare r public.nx_rooms;
begin
  if p_private is null then raise exception 'bad access mode'; end if;
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null or r.kind <> 'talk' or r.status = 'closed' then raise exception 'room closed'; end if;
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  update public.nx_rooms set is_private = p_private, updated_at = now() where id = p_room;
end $$;

create or replace function public.nx_room_invite(p_room uuid, p_user uuid)
returns void language plpgsql security definer set search_path = public as $$
declare r public.nx_rooms;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null or r.kind <> 'talk' or r.status = 'closed' then raise exception 'room closed'; end if;
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  -- nx_invites.to_user has no profile FK: a friend with an auth account may
  -- not yet have opened the games project / acquired a mirror profile.
  -- The joining phone calls nx_g_hello before accepting the invitation.
  if p_user is null or p_user = auth.uid() then raise exception 'invalid invitee'; end if;
  if exists(select 1 from public.nx_room_bans b where b.room_id = p_room and b.user_id = p_user) then
    raise exception 'unblock this member before inviting';
  end if;
  if exists(select 1 from public.nx_invites i where i.room_id = p_room and i.to_user = p_user
    and i.kind = 'talk' and i.status = 'pending' and i.created_at > now() - interval '2 minutes') then return; end if;
  insert into public.nx_invites(from_user, to_user, kind, room_id, payload)
  values (auth.uid(), p_user, 'talk', p_room, jsonb_build_object('room_id', p_room,
    'game', coalesce(r.selected_game,r.game), 'title', r.title,
    'from_name', (select username from public.profiles where id = auth.uid())));
end $$;

create or replace function public.nx_room_kick(p_room uuid, p_user uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare r public.nx_rooms;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null or r.kind <> 'talk' or r.status = 'closed' then raise exception 'room closed'; end if;
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  if p_user is null or p_user = auth.uid() then raise exception 'cannot remove manager'; end if;
  if r.status = 'playing' and exists (select 1 from public.nx_matches m
    where m.id = r.match_id and m.status = 'playing' and p_user = any(m.players)) then
    raise exception 'cannot remove an active match player';
  end if;
  if not exists(select 1 from public.nx_room_members m where m.room_id = p_room and m.user_id = p_user) then
    return jsonb_build_object('ok', false, 'reason', 'not_a_member');
  end if;
  insert into public.nx_room_bans(room_id, user_id, by_user) values (p_room, p_user, auth.uid())
    on conflict (room_id, user_id) do update set by_user = excluded.by_user, created_at = now();
  if r.status = 'lobby' then
    delete from public.nx_room_game_players where room_id = p_room and user_id = p_user;
    perform public._nx_compact_room_seats(p_room);
  end if;
  delete from public.nx_room_players where room_id = p_room and user_id = p_user;
  delete from public.nx_room_members where room_id = p_room and user_id = p_user;
  update public.nx_rooms set updated_at = now() where id = p_room;
  return jsonb_build_object('ok', true);
end $$;

create or replace function public.nx_room_unban(p_room uuid, p_user uuid)
returns void language plpgsql security definer set search_path = public as $$
declare r public.nx_rooms;
begin
  select * into r from public.nx_rooms where id = p_room for update;
  if r.id is null or r.kind <> 'talk' or r.status = 'closed' then raise exception 'room closed'; end if;
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  delete from public.nx_room_bans where room_id = p_room and user_id = p_user;
end $$;

create or replace function public.nx_room_bans_list(p_room uuid)
returns table(user_id uuid, username text, created_at timestamptz)
language plpgsql stable security definer set search_path = public as $$
begin
  if public.nx_room_member_role(p_room) is distinct from 'manager' then raise exception 'manager only'; end if;
  return query select b.user_id, p.username, b.created_at
    from public.nx_room_bans b left join public.profiles p on p.id = b.user_id
   where b.room_id = p_room order by b.created_at desc;
end $$;

revoke all on function public.nx_join_room(text,uuid), public.nx_room_leave(uuid), public.nx_room_join(uuid,text), public.nx_room_select_players(uuid,uuid[]),
  public.nx_room_set_role(uuid,uuid,text), public.nx_room_set_access(uuid,boolean),
  public.nx_room_invite(uuid,uuid), public.nx_room_kick(uuid,uuid),
  public.nx_room_unban(uuid,uuid), public.nx_room_bans_list(uuid)
from public, anon, authenticated;
grant execute on function public.nx_join_room(text,uuid), public.nx_room_leave(uuid), public.nx_room_join(uuid,text), public.nx_room_select_players(uuid,uuid[]),
  public.nx_room_set_role(uuid,uuid,text), public.nx_room_set_access(uuid,boolean),
  public.nx_room_invite(uuid,uuid), public.nx_room_kick(uuid,uuid),
  public.nx_room_unban(uuid,uuid), public.nx_room_bans_list(uuid) to authenticated, service_role;
