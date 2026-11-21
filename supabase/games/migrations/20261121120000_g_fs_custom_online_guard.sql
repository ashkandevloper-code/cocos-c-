-- Finger Soccer photo teams are bought on the separate auth/main project.
-- Until the games project can validate that project's protected inventory,
-- reject custom picks on the SERVER in online matches. Client-side hiding is
-- insufficient: crafted room preferences and setup/tpick moves must fail too.
-- This is cosmetic only; no match, lobby or existing result is modified.
create or replace function public._nx_fs_no_online_custom() returns trigger
language plpgsql security definer set search_path = public as $$
begin
  if tg_table_name = 'nx_room_game_players' then
    if (coalesce(new.prefs ->> 'team', '') = 'custom'
        or coalesce(new.prefs ->> 'team', '') like 'custom:%')
       and exists (select 1 from public.nx_rooms r where r.id = new.room_id
                   and coalesce(r.selected_game, r.game) = 'finger_soccer') then
      raise exception 'custom photo team is not available in online matches';
    end if;
  elsif tg_table_name = 'nx_match_moves' then
    if (new.move ->> 'a') in ('setup', 'tpick')
       and (coalesce(new.move ->> 'team', '') = 'custom'
            or coalesce(new.move ->> 'team', '') like 'custom:%')
       and exists (select 1 from public.nx_matches m
                    where m.id = new.match_id and m.game = 'finger_soccer') then
      raise exception 'custom photo team is not available in online matches';
    end if;
  end if;
  return new;
end $$;
revoke all on function public._nx_fs_no_online_custom() from public, anon, authenticated;

drop trigger if exists nx_fs_no_custom_room_prefs on public.nx_room_game_players;
create trigger nx_fs_no_custom_room_prefs before insert or update of prefs
on public.nx_room_game_players for each row
execute function public._nx_fs_no_online_custom();

drop trigger if exists nx_fs_no_custom_match_picks on public.nx_match_moves;
create trigger nx_fs_no_custom_match_picks before insert
on public.nx_match_moves for each row
execute function public._nx_fs_no_online_custom();
