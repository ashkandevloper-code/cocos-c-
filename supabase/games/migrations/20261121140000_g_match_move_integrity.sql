-- NEXORA PRO · follow-up to the requested result-consensus and move-log fixes.
-- GAMES project only. Without this guard, any match participant could log a
-- `forfeit` for another seat (or wrap a `move` as `system`) and then influence
-- the supposedly consensual winner. No match/result data is deleted here.
-- This is NOT server replay of the rules of all 16 games.

-- The base schema exposed all match rows (including a private room's id and
-- game seed) to every signed-in account. Lobbies still broadcast only their
-- permitted room state; match state is visible to its players and room staff.
drop policy if exists nx_matches_read on public.nx_matches;
create policy nx_matches_read on public.nx_matches for select to authenticated
  using (public.nx_in_match(id) or public.nx_has_perm('rooms'));

-- Realtime presence is a UI signal, NOT proof that an opponent is absent.
-- Only the account whose UUID occupies a match seat may refresh its heartbeat.
-- A host cannot forfeit a connected but idle player based on an old move log.
create table if not exists public.nx_match_heartbeats (
  match_id uuid not null references public.nx_matches(id) on delete cascade,
  user_id uuid not null references public.profiles(id) on delete cascade,
  seen_at timestamptz not null default now(),
  primary key (match_id, user_id)
);
alter table public.nx_match_heartbeats enable row level security;
revoke all on table public.nx_match_heartbeats from public, anon, authenticated;
grant all on table public.nx_match_heartbeats to service_role;

create or replace function public.nx_match_ping(p_match uuid)
returns void language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); m public.nx_matches;
begin
  select * into m from public.nx_matches where id = p_match for update;
  if me is null or m.id is null or m.status <> 'playing' or not (me = any(m.players)) then
    raise exception 'not an active match player';
  end if;
  if exists(select 1 from public.nx_match_moves mv where mv.match_id = p_match
       and mv.kind = 'forfeit' and mv.seat = array_position(m.players, me) - 1) then return; end if;
  insert into public.nx_match_heartbeats(match_id, user_id, seen_at)
    values (p_match, me, now())
    on conflict (match_id, user_id) do update set seen_at = excluded.seen_at;
end $$;
revoke all on function public.nx_match_ping(uuid) from public, anon, authenticated;
grant execute on function public.nx_match_ping(uuid) to authenticated, service_role;

-- A disconnected seat 0 must not freeze the authority forever. The lowest
-- surviving seat with a recent authenticated heartbeat or logged input takes
-- over; the caller is included for old clients during a rolling upgrade.
create or replace function public._nx_active_authority(p_match public.nx_matches, p_caller integer)
returns integer language sql stable security definer set search_path = public as $$
  select min(s) from generate_series(0, cardinality(p_match.players) - 1) s
    where not exists (select 1 from public.nx_match_moves mv
      where mv.match_id = p_match.id and mv.kind = 'forfeit' and mv.seat = s)
      and ((s = 0 and p_match.started_at > now() - interval '40 seconds')
      or s = p_caller or exists (select 1 from public.nx_match_heartbeats h
        where h.match_id = p_match.id and h.user_id = p_match.players[s + 1]
          and h.seen_at > now() - interval '35 seconds')
      or exists (select 1 from public.nx_match_moves mv
        where mv.match_id = p_match.id and mv.by_user = p_match.players[s + 1]
          and mv.created_at > now() - interval '35 seconds'))
$$;
revoke all on function public._nx_active_authority(public.nx_matches,integer) from public, anon, authenticated;

create or replace function public._nx_match_grace_sec(p_match public.nx_matches)
returns integer language sql immutable set search_path = public as $$
  select greatest(40, least(300, case
    when (p_match.settings ->> 'grace_sec') ~ '^[0-9]{1,3}$'
      then (p_match.settings ->> 'grace_sec')::integer
    else 40 end))
$$;
revoke all on function public._nx_match_grace_sec(public.nx_matches) from public, anon, authenticated;

-- A team match with every seat forfeit is abandoned, not an invented draw.
create or replace function public._nx_close_if_all_forfeited(p_match uuid)
returns boolean language plpgsql security definer set search_path = public as $$
declare m public.nx_matches;
begin
  select * into m from public.nx_matches where id = p_match for update;
  if m.id is null or m.status <> 'playing' or exists (
    select 1 from generate_series(0, cardinality(m.players) - 1) s
      where not exists(select 1 from public.nx_match_moves mv
        where mv.match_id = p_match and mv.kind = 'forfeit' and mv.seat = s)
  ) then return false; end if;
  update public.nx_matches set status = 'abandoned', winner_seats = null,
    reason = 'all_forfeited', ended_at = now() where id = p_match;
  update public.nx_rooms set status = 'lobby', updated_at = now()
    where id = m.room_id and match_id = p_match;
  update public.nx_room_players set ready = false where room_id = m.room_id and seat > 0;
  return true;
end $$;
revoke all on function public._nx_close_if_all_forfeited(uuid) from public, anon, authenticated;

create or replace function public.nx_push_move(
  p_match uuid, p_seq integer, p_kind text, p_move jsonb, p_hash text,
  p_seat integer default null
) returns jsonb language plpgsql security definer set search_path = public as $$
declare
  me uuid := auth.uid(); m nx_matches; caller_seat int; effective_seat int;
  k text := coalesce(p_kind, 'move'); authority int; n int;
begin
  if me is null then raise exception 'not signed in'; end if;
  select * into m from nx_matches where id = p_match for update;
  if m.id is null then raise exception 'match not found'; end if;
  caller_seat := array_position(m.players, me) - 1;
  if caller_seat is null then raise exception 'not a player'; end if;
  if m.status <> 'playing' then
    return jsonb_build_object('ok', false, 'reason', 'over', 'last_seq', m.last_seq);
  end if;
  if p_seq is distinct from m.last_seq + 1 then
    return jsonb_build_object('ok', false, 'reason', 'seq', 'last_seq', m.last_seq);
  end if;
  if k not in ('move','snap','timeout','system','forfeit') or
      jsonb_typeof(coalesce(p_move, '{}'::jsonb)) <> 'object' then
    return jsonb_build_object('ok', false, 'reason', 'bad_move');
  end if;
  if pg_column_size(coalesce(p_move, '{}'::jsonb)) > 16000 or length(coalesce(p_hash, '')) > 128 then
    return jsonb_build_object('ok', false, 'reason', 'too_large');
  end if;
  if k = 'system' and coalesce(p_move ->> '_k','') not in ('move','timeout') then
    return jsonb_build_object('ok', false, 'reason', 'bad_wire_kind');
  end if;
  n := cardinality(m.players);
  if p_seat is not null and (p_seat < 0 or p_seat >= n) then
    raise exception 'bad seat';
  end if;
  effective_seat := coalesce(p_seat, caller_seat);
  if exists(select 1 from nx_match_moves mv where mv.match_id = p_match
      and mv.kind = 'forfeit' and mv.seat = caller_seat) then
    return jsonb_build_object('ok', false, 'reason', 'already_forfeited');
  end if;
  if k = 'forfeit' then
    -- The logged leave used by Finger Soccer is ALWAYS this caller's own
    -- seat. Host-initiated absent-player forfeits use nx_forfeit (with checks).
    if effective_seat <> caller_seat or p_move ->> 'a' is distinct from 'leave' or
       p_move ->> 'seat' is distinct from caller_seat::text then
      raise exception 'cannot forfeit another seat via move log';
    end if;
  elsif effective_seat <> caller_seat then
    -- Only a server-designated authority may post a timeout for an absent
    -- seat. Never accept a foreign snapshot, ordinary move or a wrapped move.
    if k not in ('timeout','system') or
       (k = 'system' and p_move ->> '_k' <> 'timeout') then
      raise exception 'seat mismatch';
    end if;
    authority := public._nx_active_authority(m, caller_seat);
    if caller_seat is distinct from authority then raise exception 'authority only'; end if;
    -- Only the four games with client-side absent-player skips may write a
    -- timeout for another seat, and only their no-choice/pass payload shape.
    -- Server rule replay/timing is still a separate release requirement.
    if not coalesce((
      (m.game = 'carrom' and p_move ->> 'a' = 'pass' and p_move ->> 'tn' ~ '^[0-9]+$') or
      (m.game = 'quiz' and p_move ->> 'a' = 'ans' and p_move ->> 'c' = '-1'
         and p_move ->> 'ms' = '15000' and p_move ->> 'r' ~ '^[0-9]+$') or
      (m.game = 'mafia' and p_move ->> 'a' in ('night','nom','final')
         and p_move -> 't' = 'null'::jsonb and p_move ->> 'd' ~ '^[0-9]+$') or
      (m.game = 'spy_mafia' and p_move ->> 'a' = 'vote' and p_move ->> 'v' = '-1')
    ), false) then return jsonb_build_object('ok', false, 'reason', 'bad_timeout'); end if;
  end if;
  if k = 'snap' then
    if m.game not in ('carrom', 'billiards') or
       caller_seat is distinct from public._nx_active_authority(m, caller_seat) then
      raise exception 'snapshot authority only';
    end if;
  end if;

  insert into nx_match_moves(match_id, seq, seat, by_user, kind, move, hash)
  values (p_match, p_seq, effective_seat, me, k, coalesce(p_move, '{}'::jsonb), p_hash);
  update nx_matches set last_seq = p_seq, state_hash = coalesce(p_hash, state_hash),
         turn_seat = effective_seat, turn_deadline = now() + interval '45 seconds'
   where id = p_match;
  if k = 'forfeit' then
    -- Compatibility for older app versions that log their own leave instead
    -- of calling nx_forfeit; a two-seat match must settle the same way.
    if n = 2 then
      perform public.nx_finish_match_as(p_match, array[1 - effective_seat], effective_seat);
    else
      perform public._nx_close_if_all_forfeited(p_match);
    end if;
  end if;
  return jsonb_build_object('ok', true, 'seq', p_seq);
end $$;
revoke all on function public.nx_push_move(uuid,integer,text,jsonb,text,integer)
  from public, anon, authenticated;
grant execute on function public.nx_push_move(uuid,integer,text,jsonb,text,integer)
  to authenticated, service_role;

-- The original result-consensus function is replaced below with the SAME
-- signature. It now derives forfeits from the logged seat, rejects claiming a
-- forfeited seat as winner, checks score-array shape, and refuses client-chosen
-- reasons. It still cannot detect two colluding clients inventing a score.

CREATE OR REPLACE FUNCTION public.nx_finish_match(p_match uuid, p_winners integer[], p_scores integer[], p_hash text DEFAULT NULL::text, p_reason text DEFAULT 'finished'::text)
 RETURNS jsonb
 LANGUAGE plpgsql
 SECURITY DEFINER
 SET search_path TO 'public'
AS $function$
declare
  me uuid := auth.uid();
  m nx_matches;
  n int;
  w int[];
  forfeited int[];
  required uuid[];
  missing uuid[];
  n_reports int;
  n_winner_sets int;
  n_hashes int;
  final_scores int[];
  k int;
  u uuid;
  gone boolean := true;
begin
  select * into m from nx_matches where id = p_match for update;
  if m.id is null then raise exception 'match not found'; end if;
  if me is null or not (me = any(m.players)) then raise exception 'not a player'; end if;
  if m.status <> 'playing' then
    return jsonb_build_object('ok', true, 'already', true, 'winners', m.winner_seats, 'status', m.status);
  end if;
  n := coalesce(array_length(m.players, 1), 0);
  if p_reason is distinct from 'finished' then
    return jsonb_build_object('ok', false, 'error', 'bad_reason');
  end if;
  if p_scores is not null and (cardinality(p_scores) <> n or exists (
    select 1 from unnest(p_scores) x where x is null or x < 0 or x > 100000
  )) then return jsonb_build_object('ok', false, 'error', 'bad_scores'); end if;

  -- normalized winners: distinct, sorted, valid seats only
  if exists (select 1 from unnest(coalesce(p_winners, '{}'::int[])) x where x is null or x < 0 or x >= n) then
    return jsonb_build_object('ok', false, 'error', 'bad_winners');
  end if;
  select coalesce(array_agg(distinct x order by x), '{}'::int[]) into w from unnest(coalesce(p_winners, '{}'::int[])) x;

  -- Count the seat actually logged by the server, not an attacker-selected
  -- move['seat']; nx_push_move only permits a self-forfeit. nx_forfeit checks
  -- an absent opponent before writing any host-forfeited seat.
  select coalesce(array_agg(distinct mv.seat), '{}'::int[]) into forfeited
    from nx_match_moves mv where mv.match_id = p_match and mv.kind = 'forfeit';
  if (array_position(m.players, me) - 1) = any(forfeited) then
    return jsonb_build_object('ok', false, 'error', 'forfeited_reporter');
  end if;
  if exists(select 1 from unnest(w) x where x = any(forfeited)) then
    return jsonb_build_object('ok', false, 'error', 'forfeited_winner');
  end if;

  -- first report per player is kept (a phone cannot change its claim)
  insert into nx_match_reports(match_id, user_id, winners, scores, state_hash, reason)
  values (p_match, me, w, p_scores, nullif(left(p_hash, 128), ''), 'finished')
  on conflict (match_id, user_id) do nothing;

  -- required reporters: every player whose seat has not forfeited
  select coalesce(array_agg(m.players[g]), '{}'::uuid[]) into required
    from generate_series(1, n) g
   where m.players[g] is not null and not ((g - 1) = any(forfeited));
  select coalesce(array_agg(x), '{}'::uuid[]) into missing
    from unnest(required) x
   where not exists (select 1 from nx_match_reports r where r.match_id = p_match and r.user_id = x);

  -- A player may have reported before later forfeiting. Their former claim
  -- cannot veto or dictate the surviving players' settlement.
  select count(*), count(distinct winners), count(distinct state_hash)
    into n_reports, n_winner_sets, n_hashes
    from nx_match_reports where match_id = p_match and user_id = any(required);

  -- conflicting claims or malformed historical reports -> disputed, no rewards
  if n_winner_sets > 1 or n_hashes > 1 or exists (
    select 1 from nx_match_reports r cross join lateral unnest(r.winners) winner
    where r.match_id = p_match and r.user_id = any(required) and winner = any(forfeited)
  ) or exists (
    select 1 from nx_match_reports r where r.match_id = p_match and r.user_id = any(required)
      and r.scores is not null and (cardinality(r.scores) <> n or exists (
        select 1 from unnest(r.scores) x where x is null or x < 0 or x > 100000
      ))
  ) then
    update nx_matches set status = 'disputed', winner_seats = null, reason = 'disputed', ended_at = now()
     where id = p_match;
    update nx_rooms set status = 'lobby', updated_at = now() where id = m.room_id and match_id = m.id;
    update nx_room_players set ready = false where room_id = m.room_id and seat > 0;
    return jsonb_build_object('ok', true, 'disputed', true, 'winners', null);
  end if;

  if coalesce(array_length(required, 1), 0) = 0 then
    -- Everyone left. There is no legitimate remaining winner or draw prize.
    update nx_matches set status = 'abandoned', winner_seats = null,
      reason = 'all_forfeited', ended_at = now() where id = p_match;
    update nx_rooms set status = 'lobby', updated_at = now() where id = m.room_id and match_id = m.id;
    update nx_room_players set ready = false where room_id = m.room_id and seat > 0;
    return jsonb_build_object('ok', true, 'abandoned', true, 'winners', null);
  end if;
  if coalesce(array_length(missing, 1), 0) > 0 then
    -- lone-survivor rule: everyone who has not reported must be gone
    if now() <= m.turn_deadline or
       m.started_at > now() - make_interval(secs => public._nx_match_grace_sec(m)) then
      gone := false;
    else
      foreach u in array missing loop
        if exists (select 1 from nx_match_moves mv where mv.match_id = p_match and mv.by_user = u
                    and mv.created_at > now() - make_interval(secs => public._nx_match_grace_sec(m)))
           or exists (select 1 from nx_match_heartbeats h where h.match_id = p_match and h.user_id = u
                    and h.seen_at > now() - make_interval(secs => public._nx_match_grace_sec(m)))
           or exists (select 1 from nx_room_players rp where rp.room_id = m.room_id and rp.user_id = u
                    and rp.last_seen > now() - make_interval(secs => public._nx_match_grace_sec(m))) then
          gone := false;
          exit;
        end if;
      end loop;
    end if;
    if not gone then
      return jsonb_build_object('ok', true, 'pending', true, 'reports', n_reports,
                                'needed', coalesce(array_length(required, 1), 0));
    end if;
  end if;

  -- finalize with the agreed winners; scores = element-wise minimum over
  -- *surviving* reports (conservative; prior forfeits cannot set a score).
  select r.winners into w from nx_match_reports r
    where r.match_id = p_match and r.user_id = any(required)
    order by r.created_at, r.user_id limit 1;
  final_scores := null;
  if not exists (select 1 from nx_match_reports r
      where r.match_id = p_match and r.user_id = any(required) and r.scores is null) then
    final_scores := '{}'::int[];
    for k in 1..n loop
      final_scores := final_scores || (select min(r.scores[k])
        from nx_match_reports r where r.match_id = p_match and r.user_id = any(required));
    end loop;
  end if;
  update nx_matches set status = 'finished', winner_seats = w, scores = final_scores,
         reason = 'finished',
         state_hash = coalesce((select max(r.state_hash) from nx_match_reports r
                                where r.match_id = p_match and r.user_id = any(required)), state_hash),
         ended_at = now()
   where id = p_match returning * into m;
  perform _nx_apply_result(m, w, final_scores);
  update nx_rooms set status = 'lobby', updated_at = now() where id = m.room_id and match_id = m.id;
  update nx_room_players set ready = false where room_id = m.room_id and seat > 0;
  return jsonb_build_object('ok', true, 'winners', w);
end $function$;

-- Self-withdrawal is always permitted while playing. For a different seat the
-- current authority must prove absence for the game's reconnect grace. A live
-- heartbeat or match input blocks a hostile host's attempted forfeit.
create or replace function public.nx_forfeit(p_match uuid, p_seat integer default null)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); m public.nx_matches; mine int; target int; grace interval;
begin
  select * into m from public.nx_matches where id = p_match for update;
  if m.id is null then return jsonb_build_object('ok', false, 'reason', 'not_found'); end if;
  mine := array_position(m.players, me) - 1;
  if me is null or mine is null then raise exception 'not a player'; end if;
  target := coalesce(p_seat, mine);
  if target < 0 or target >= cardinality(m.players) then
    return jsonb_build_object('ok', false, 'reason', 'bad_seat');
  end if;
  if exists (select 1 from public.nx_match_moves mv
    where mv.match_id = p_match and mv.kind = 'forfeit' and mv.seat = target) then
    return jsonb_build_object('ok', true, 'already', true, 'seat', target);
  end if;
  if m.status <> 'playing' then return jsonb_build_object('ok', false, 'reason', 'over'); end if;
  if target <> mine then
    if mine is distinct from public._nx_active_authority(m, mine) then raise exception 'authority only'; end if;
    if exists(select 1 from public.nx_match_reports r where r.match_id = p_match
        and r.user_id = m.players[target + 1]) then
      return jsonb_build_object('ok', false, 'reason', 'reported');
    end if;
    grace := make_interval(secs => public._nx_match_grace_sec(m));
    if m.started_at > now() - grace or
       exists(select 1 from public.nx_match_heartbeats h where h.match_id = p_match
         and h.user_id = m.players[target + 1] and h.seen_at > now() - grace) or
       exists(select 1 from public.nx_match_moves mv where mv.match_id = p_match
         and mv.by_user = m.players[target + 1] and mv.created_at > now() - grace) or
       exists(select 1 from public.nx_room_players rp where rp.room_id = m.room_id
         and rp.user_id = m.players[target + 1] and rp.last_seen > now() - grace)
    then return jsonb_build_object('ok', false, 'reason', 'active_or_reconnecting'); end if;
  end if;
  insert into public.nx_match_moves(match_id, seq, seat, by_user, kind, move)
    values (p_match, m.last_seq + 1, target, me, 'forfeit', jsonb_build_object('seat', target));
  update public.nx_matches set last_seq = m.last_seq + 1 where id = p_match;
  if cardinality(m.players) = 2 then
    return public.nx_finish_match_as(p_match, array[1 - target], target);
  end if;
  if public._nx_close_if_all_forfeited(p_match) then
    return jsonb_build_object('ok', true, 'abandoned', true, 'seat', target);
  end if;
  return jsonb_build_object('ok', true, 'seat', target);
end $$;
revoke all on function public.nx_forfeit(uuid,integer) from public, anon, authenticated;
grant execute on function public.nx_forfeit(uuid,integer) to authenticated, service_role;
