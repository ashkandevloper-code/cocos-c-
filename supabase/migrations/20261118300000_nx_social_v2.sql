-- NEXORA social v2: profiles, follow / follow requests, private accounts, avatars, DMs, notifications.
-- Builds on the live tables: profiles, follows, nx_follow_requests, nx_social_notifications,
-- nx_chats / nx_chat_members / nx_messages, storage bucket "avatars". Idempotent; safe to re-run.
-- All timestamps are timestamptz (stored UTC); clients convert to the device time zone.

begin;

-- ---------------------------------------------------------------- profiles
alter table public.profiles add column if not exists updated_at timestamptz not null default now();
alter table public.profiles add column if not exists avatar_path text;          -- object key in bucket "avatars"
create unique index if not exists profiles_username_lower_key on public.profiles (lower(username));
create index if not exists profiles_username_prefix_idx on public.profiles (lower(username) text_pattern_ops);
create index if not exists profiles_display_name_idx on public.profiles (lower(display_name) text_pattern_ops);

-- ---------------------------------------------------------------- follows
create index if not exists follows_followee_created_idx on public.follows (followee, created_at desc);
create index if not exists follows_follower_created_idx on public.follows (follower, created_at desc);
create index if not exists nx_follow_requests_target_idx on public.nx_follow_requests (target, status, created_at desc);
create index if not exists nx_social_notifications_user_idx on public.nx_social_notifications (user_id, created_at desc);
create index if not exists nx_social_notifications_unread_idx on public.nx_social_notifications (user_id) where read_at is null;
create index if not exists nx_messages_chat_created_idx on public.nx_messages (chat_id, created_at desc);

-- Direct REST inserts may not bypass private accounts: a follow row needs a public target or an accepted request.
drop policy if exists follow on public.follows;
create policy follow on public.follows for insert to authenticated
  with check (
    follower = auth.uid()
    and (
      not exists (select 1 from public.profiles p where p.id = followee and p.is_private)
      or exists (select 1 from public.nx_follow_requests r
                 where r.requester = follower and r.target = followee and r.status = 'accepted')
    )
  );

-- ---------------------------------------------------------------- notifications kinds
alter table public.nx_social_notifications drop constraint if exists nx_social_notifications_kind_check;
alter table public.nx_social_notifications add constraint nx_social_notifications_kind_check
  check (kind = any (array['follow','follow_request','follow_accepted','message','like','comment','mention']));

-- ---------------------------------------------------------------- rate limiting
create table if not exists public.nx_rate_events (
  user_id uuid not null references auth.users(id) on delete cascade,
  action  text not null,
  at      timestamptz not null default now()
);
create index if not exists nx_rate_events_idx on public.nx_rate_events (user_id, action, at desc);
alter table public.nx_rate_events enable row level security;  -- no policies: only SECURITY DEFINER code touches it

create or replace function public._nx_rate(p_action text, p_max int, p_window interval)
returns void language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); n int;
begin
  if me is null then return; end if;
  select count(*) into n from nx_rate_events where user_id = me and action = p_action and at > now() - p_window;
  if n >= p_max then raise exception 'rate_limited' using errcode = 'P0001'; end if;
  insert into nx_rate_events(user_id, action) values (me, p_action);
  delete from nx_rate_events where user_id = me and action = p_action and at < now() - interval '1 day';
end $$;
revoke all on function public._nx_rate(text, int, interval) from public, anon, authenticated;

-- ---------------------------------------------------------------- helpers
create or replace function public._nx_blocked(a uuid, b uuid)
returns boolean language sql stable security definer set search_path = public as $$
  select exists (select 1 from user_blocks where (blocker = a and blocked = b) or (blocker = b and blocked = a));
$$;

-- Can the signed-in user see [p_user]'s posts and follower lists?
create or replace function public.nx_social_can_view(p_user uuid)
returns boolean language sql stable security definer set search_path = public as $$
  select p_user = auth.uid()
      or (not public._nx_blocked(auth.uid(), p_user)
          and (not coalesce((select is_private from profiles where id = p_user), false)
               or exists (select 1 from follows where follower = auth.uid() and followee = p_user)));
$$;


-- Card used by every list (followers, search, notifications, chat header).
create or replace function public._nx_user_card(p_user uuid)
returns jsonb language sql stable security definer set search_path = public as $$
  select jsonb_build_object(
    'id', p.id,
    'username', p.username,
    'display_name', coalesce(nullif(p.display_name, ''), p.username),
    'avatar_url', nullif(coalesce(p.avatar_url, ''), ''),
    'is_private', p.is_private,
    'is_following', exists (select 1 from follows f where f.follower = auth.uid() and f.followee = p.id),
    'is_pending', exists (select 1 from nx_follow_requests q where q.requester = auth.uid() and q.target = p.id and q.status = 'pending'),
    'is_me', p.id = auth.uid())
  from profiles p where p.id = p_user;
$$;

-- ---------------------------------------------------------------- profile
create or replace function public.nx_social_profile(p_user uuid)
returns jsonb language plpgsql stable security definer set search_path = public as $$
declare r record; me uuid := auth.uid(); v boolean;
begin
  if me is null then raise exception 'not_signed_in'; end if;
  if p_user is null then raise exception 'bad_user'; end if;
  select p.* into r from profiles p where p.id = p_user;
  if not found then raise exception 'profile_not_found'; end if;
  v := public.nx_social_can_view(p_user);
  return jsonb_build_object(
    'id', r.id,
    'username', coalesce(r.username, ''),
    'display_name', coalesce(r.display_name, ''),
    'avatar_url', nullif(coalesce(r.avatar_url, ''), ''),
    'bio', coalesce(r.bio, ''),
    'website', coalesce(r.website, ''),
    'location', coalesce(r.location, ''),
    'is_private', r.is_private,
    'created_at', r.created_at,
    'last_active', r.online_at,
    'followers', (select count(*)::int from follows f where f.followee = r.id),
    'following', (select count(*)::int from follows f where f.follower = r.id),
    'posts', (select count(*)::int from nx_posts x where x.author = r.id and x.kind = 'post'),
    'is_me', r.id = me,
    'is_following', exists (select 1 from follows f where f.follower = me and f.followee = r.id),
    'follows_me', exists (select 1 from follows f where f.follower = r.id and f.followee = me),
    'is_pending', exists (select 1 from nx_follow_requests q where q.requester = me and q.target = r.id and q.status = 'pending'),
    'is_blocked', public._nx_blocked(me, r.id),
    'can_view', v,
    'pending_requests', case when r.id = me then (select count(*)::int from nx_follow_requests q where q.target = me and q.status = 'pending') else 0 end);
end $$;

-- Text fields only. Avatar has its own call (nx_social_set_avatar).
create or replace function public.nx_social_save_profile(p_display_name text, p_username text, p_bio text,
                                                        p_website text, p_location text)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); n text := lower(trim(coalesce(p_username, ''))); w text := trim(coalesce(p_website, ''));
begin
  if me is null then raise exception 'not_signed_in'; end if;
  perform public._nx_rate('profile_save', 20, interval '10 minutes');
  if char_length(n) < 3 or char_length(n) > 20 or n !~ '^[a-z0-9_.]+$' then raise exception 'bad_username'; end if;
  if exists (select 1 from profiles where lower(username) = n and id <> me) then raise exception 'username_taken'; end if;
  if char_length(trim(coalesce(p_bio, ''))) > 160 then raise exception 'bio_too_long'; end if;
  if w <> '' and w !~* '^(https?://)?[a-z0-9.-]+\.[a-z]{2,}(/[^\s<>"]*)?$' then raise exception 'bad_website'; end if;
  update profiles set
    username = n,
    display_name = left(regexp_replace(trim(coalesce(p_display_name, '')), '[<>]', '', 'g'), 40),
    bio = regexp_replace(trim(coalesce(p_bio, '')), '[<>]', '', 'g'),
    website = left(w, 120),
    location = left(regexp_replace(trim(coalesce(p_location, '')), '[<>]', '', 'g'), 60),
    updated_at = now()
  where id = me;
  return public.nx_social_profile(me);
end $$;

-- Back-compat signature used by older clients: same rules; an empty avatar url keeps the current avatar.
create or replace function public.nx_social_update_profile(p_display_name text, p_username text, p_bio text,
                                                          p_website text, p_location text, p_avatar_url text)
returns jsonb language plpgsql security definer set search_path = public as $$
begin
  return public.nx_social_save_profile(p_display_name, p_username, p_bio, p_website, p_location);
end $$;

-- p_path: "<uid>/<file>.jpg|png|webp" already uploaded to bucket "avatars"; null = remove the avatar.
create or replace function public.nx_social_set_avatar(p_path text)
returns jsonb language plpgsql security definer set search_path = public, storage as $$
declare me uuid := auth.uid(); old text; base text;
begin
  if me is null then raise exception 'not_signed_in'; end if;
  perform public._nx_rate('avatar', 20, interval '10 minutes');
  select avatar_path into old from public.profiles where id = me;
  if p_path is null or p_path = '' then
    update public.profiles set avatar_path = null, avatar_url = null, updated_at = now() where id = me;
  else
    if p_path !~ ('^' || me::text || '/[A-Za-z0-9_-]+\.(jpg|jpeg|png|webp)$') then raise exception 'bad_avatar_path'; end if;
    if not exists (select 1 from storage.objects o where o.bucket_id = 'avatars' and o.name = p_path) then
      raise exception 'avatar_missing';
    end if;
    select coalesce(nullif(current_setting('nx.public_url', true), ''), 'https://eisstodgncimypdawisy.supabase.co') into base;
    update public.profiles set avatar_path = p_path,
      avatar_url = base || '/storage/v1/object/public/avatars/' || p_path,
      updated_at = now() where id = me;
  end if;
  return jsonb_build_object('profile', public.nx_social_profile(me), 'old_path', old);
end $$;

create or replace function public.nx_social_set_privacy(p_private boolean)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid();
begin
  if me is null then raise exception 'not_signed_in'; end if;
  update profiles set is_private = coalesce(p_private, false), updated_at = now() where id = me;
  if not coalesce(p_private, false) then
    -- going public accepts every pending request
    insert into follows(follower, followee)
      select requester, me from nx_follow_requests where target = me and status = 'pending'
      on conflict do nothing;
    update nx_follow_requests set status = 'accepted', updated_at = now() where target = me and status = 'pending';
  end if;
  return public.nx_social_profile(me);
end $$;

-- ---------------------------------------------------------------- follow
create or replace function public.nx_social_toggle_follow(p_target uuid, p_follow boolean)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid(); priv boolean;
begin
  if me is null then raise exception 'not_signed_in'; end if;
  if p_target is null or p_target = me then raise exception 'bad_target'; end if;
  select is_private into priv from profiles where id = p_target;
  if not found then raise exception 'profile_not_found'; end if;
  perform public._nx_rate('follow', 60, interval '10 minutes');
  if not coalesce(p_follow, false) then
    delete from follows where follower = me and followee = p_target;
    update nx_follow_requests set status = 'cancelled', updated_at = now()
      where requester = me and target = p_target and status = 'pending';
    delete from nx_social_notifications where user_id = p_target and actor = me and kind = 'follow_request' and read_at is null;
  else
    if public._nx_blocked(me, p_target) then raise exception 'blocked'; end if;
    if exists (select 1 from follows where follower = me and followee = p_target) then
      return public.nx_social_profile(p_target);  -- already following: idempotent
    end if;
    if priv then
      insert into nx_follow_requests(requester, target, status) values (me, p_target, 'pending')
        on conflict (requester, target) do update set status = 'pending', updated_at = now()
        where nx_follow_requests.status <> 'pending';
      if not exists (select 1 from nx_social_notifications where user_id = p_target and actor = me
                     and kind = 'follow_request' and read_at is null) then
        insert into nx_social_notifications(user_id, actor, kind) values (p_target, me, 'follow_request');
      end if;
    else
      insert into follows(follower, followee) values (me, p_target) on conflict do nothing;
      if not exists (select 1 from nx_social_notifications where user_id = p_target and actor = me and kind = 'follow'
                     and created_at > now() - interval '1 hour') then
        insert into nx_social_notifications(user_id, actor, kind) values (p_target, me, 'follow');
      end if;
    end if;
  end if;
  return public.nx_social_profile(p_target);
end $$;

create or replace function public.nx_social_respond_follow_request(p_requester uuid, p_accept boolean)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid();
begin
  if me is null or p_requester is null or p_requester = me then raise exception 'bad_request'; end if;
  if not exists (select 1 from nx_follow_requests where requester = p_requester and target = me and status = 'pending') then
    raise exception 'request_not_found';
  end if;
  update nx_follow_requests set status = case when p_accept then 'accepted' else 'rejected' end, updated_at = now()
    where requester = p_requester and target = me and status = 'pending';
  if p_accept then
    insert into follows(follower, followee) values (p_requester, me) on conflict do nothing;
    insert into nx_social_notifications(user_id, actor, kind) values (p_requester, me, 'follow_accepted');
  end if;
  update nx_social_notifications set read_at = now()
    where user_id = me and actor = p_requester and kind = 'follow_request' and read_at is null;
  return public.nx_social_profile(me);
end $$;

-- Removes one of my followers (also how a private account revokes access).
create or replace function public.nx_social_remove_follower(p_user uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare me uuid := auth.uid();
begin
  if me is null then raise exception 'not_signed_in'; end if;
  delete from follows where follower = p_user and followee = me;
  update nx_follow_requests set status = 'cancelled', updated_at = now() where requester = p_user and target = me;
  return public.nx_social_profile(me);
end $$;

create or replace function public.nx_social_follow_requests(p_limit int default 50, p_offset int default 0)
returns jsonb language sql stable security definer set search_path = public as $$
  select coalesce(jsonb_agg(public._nx_user_card(q.requester) || jsonb_build_object('requested_at', q.created_at)
                            order by q.created_at desc), '[]'::jsonb)
  from (select * from nx_follow_requests where target = auth.uid() and status = 'pending'
        order by created_at desc limit least(greatest(p_limit, 1), 100) offset greatest(p_offset, 0)) q;
$$;

-- p_kind: 'followers' | 'following'
create or replace function public.nx_social_follow_list(p_user uuid, p_kind text, p_limit int default 50, p_offset int default 0)
returns jsonb language plpgsql stable security definer set search_path = public as $$
declare res jsonb;
begin
  if auth.uid() is null then raise exception 'not_signed_in'; end if;
  if not public.nx_social_can_view(p_user) then raise exception 'private_account'; end if;
  if p_kind = 'followers' then
    select coalesce(jsonb_agg(public._nx_user_card(f.follower) || jsonb_build_object('since', f.created_at) order by f.created_at desc), '[]')
      into res
      from (select * from follows where followee = p_user order by created_at desc
            limit least(greatest(p_limit, 1), 100) offset greatest(p_offset, 0)) f;
  elsif p_kind = 'following' then
    select coalesce(jsonb_agg(public._nx_user_card(f.followee) || jsonb_build_object('since', f.created_at) order by f.created_at desc), '[]')
      into res
      from (select * from follows where follower = p_user order by created_at desc
            limit least(greatest(p_limit, 1), 100) offset greatest(p_offset, 0)) f;
  else
    raise exception 'bad_kind';
  end if;
  return res;
end $$;

create or replace function public.nx_social_search(p_q text, p_limit int default 30)
returns jsonb language sql stable security definer set search_path = public as $$
  with q as (select lower(trim(regexp_replace(coalesce(p_q, ''), '[%_\\]', '', 'g'))) s)
  select coalesce(jsonb_agg(public._nx_user_card(p.id) order by (lower(p.username) = q.s) desc, lower(p.username)), '[]'::jsonb)
  from profiles p, q
  where char_length(q.s) >= 1 and not p.is_guest and p.id <> auth.uid()
    and not public._nx_blocked(auth.uid(), p.id)
    and (lower(p.username) like q.s || '%' or lower(p.display_name) like q.s || '%' or lower(p.display_name) like '% ' || q.s || '%')
  limit least(greatest(p_limit, 1), 50);
$$;

-- ---------------------------------------------------------------- posts
-- Private accounts: only followers (and the author) can read posts.
drop policy if exists nx_posts_read on public.nx_posts;
create policy nx_posts_read on public.nx_posts for select using (
  author = auth.uid()
  or ((expires_at is null or expires_at > now()) and public.nx_social_can_view(author))
);

create or replace function public.nx_social_user_posts(p_user uuid, p_limit int default 30, p_before timestamptz default null)
returns jsonb language plpgsql stable security definer set search_path = public as $$
declare res jsonb;
begin
  if auth.uid() is null then raise exception 'not_signed_in'; end if;
  if not public.nx_social_can_view(p_user) then raise exception 'private_account'; end if;
  select coalesce(jsonb_agg(jsonb_build_object('id', x.id, 'text', x.text, 'visual', x.visual, 'like_count', x.like_count,
           'comment_count', x.comment_count, 'created_at', x.created_at, 'edited_at', x.edited_at) order by x.created_at desc), '[]')
    into res
    from (select * from nx_posts where author = p_user and kind = 'post'
            and (p_before is null or created_at < p_before)
          order by created_at desc limit least(greatest(p_limit, 1), 60)) x;
  return res;
end $$;

-- ---------------------------------------------------------------- direct messages
-- Opens (or reuses) the one direct chat between me and p_user; returns the chat id and the peer card.
create or replace function public.nx_social_open_dm(p_user uuid)
returns jsonb language plpgsql security definer set search_path = public as $$
declare cid uuid;
begin
  if auth.uid() is null then raise exception 'not_signed_in'; end if;
  if p_user is null or p_user = auth.uid() then raise exception 'bad_user'; end if;
  if public._nx_blocked(auth.uid(), p_user) then raise exception 'blocked'; end if;
  perform public._nx_rate('open_dm', 60, interval '10 minutes');
  cid := public.nx_open_direct(p_user);
  return jsonb_build_object('chat_id', cid, 'peer', public._nx_user_card(p_user),
    'peer_last_read', (select last_read_at from nx_chat_members where chat_id = cid and user_id = p_user));
end $$;

create or replace function public.nx_social_send_dm(p_chat uuid, p_text text)
returns nx_messages language plpgsql security definer set search_path = public as $$
declare peer uuid; m nx_messages;
begin
  if auth.uid() is null then raise exception 'not_signed_in'; end if;
  perform public._nx_rate('dm', 30, interval '1 minute');
  select user_id into peer from nx_chat_members where chat_id = p_chat and user_id <> auth.uid() limit 1;
  if peer is not null and public._nx_blocked(auth.uid(), peer) then raise exception 'blocked'; end if;
  if char_length(trim(coalesce(p_text, ''))) = 0 then raise exception 'empty'; end if;
  m := public.nx_send_message(p_chat, 'text', p_text, '{}'::jsonb, null);
  return m;
end $$;

-- Messages oldest-first for display, with the peer's read position for "seen" ticks.
create or replace function public.nx_social_dm_messages(p_chat uuid, p_limit int default 50, p_before timestamptz default null)
returns jsonb language plpgsql stable security definer set search_path = public as $$
declare res jsonb;
begin
  if not public.nx_is_chat_member(p_chat) then raise exception 'not_member'; end if;
  select coalesce(jsonb_agg(jsonb_build_object('id', z.id, 'author', z.author, 'kind', z.kind, 'text', z.text,
           'created_at', z.created_at, 'deleted', z.deleted) order by z.created_at asc), '[]')
    into res
    from (select * from nx_messages where chat_id = p_chat and (p_before is null or created_at < p_before)
          order by created_at desc limit least(greatest(p_limit, 1), 200)) z;
  return jsonb_build_object('messages', res,
    'peer_last_read', (select max(last_read_at) from nx_chat_members where chat_id = p_chat and user_id <> auth.uid()));
end $$;

-- Message notifications: one unread notification per chat instead of one per message.
create or replace function public.nx_social_message_notification()
returns trigger language plpgsql security definer set search_path = public as $$
declare recipient uuid;
begin
  if new.author is null then return new; end if;
  for recipient in select cm.user_id from nx_chat_members cm
                   where cm.chat_id = new.chat_id and cm.user_id <> new.author and not cm.muted loop
    if exists (select 1 from nx_social_notifications n where n.user_id = recipient and n.kind = 'message'
               and n.actor = new.author and n.read_at is null) then
      update nx_social_notifications set entity_id = new.id, created_at = now()
        where user_id = recipient and kind = 'message' and actor = new.author and read_at is null;
    else
      insert into nx_social_notifications(user_id, actor, kind, entity_id) values (recipient, new.author, 'message', new.id);
    end if;
  end loop;
  return new;
end $$;

create or replace function public.nx_social_mark_chat_read(p_chat uuid)
returns void language sql security definer set search_path = public as $$
  update nx_chat_members set last_read_at = now() where chat_id = p_chat and user_id = auth.uid();
  update nx_social_notifications n set read_at = now()
    where n.user_id = auth.uid() and n.kind = 'message' and n.read_at is null
      and n.actor in (select user_id from nx_chat_members where chat_id = p_chat and user_id <> auth.uid());
$$;

-- ---------------------------------------------------------------- notifications
create or replace function public.nx_social_notifications_list(p_limit int default 50)
returns jsonb language sql stable security definer set search_path = public as $$
  select jsonb_build_object(
    'items', coalesce((select jsonb_agg(jsonb_build_object('id', n.id, 'kind', n.kind, 'entity_id', n.entity_id,
               'created_at', n.created_at, 'read', n.read_at is not null,
               'actor', case when n.actor is null then null else public._nx_user_card(n.actor) end) order by n.created_at desc)
             from (select * from nx_social_notifications where user_id = auth.uid()
                   order by created_at desc limit least(greatest(p_limit, 1), 100)) n), '[]'::jsonb),
    'unread', (select count(*)::int from nx_social_notifications where user_id = auth.uid() and read_at is null));
$$;

create or replace function public.nx_social_mark_notification(p_id uuid)
returns void language sql security definer set search_path = public as $$
  update nx_social_notifications set read_at = now() where id = p_id and user_id = auth.uid() and read_at is null;
$$;

create or replace function public.nx_social_mark_all_notifications()
returns void language sql security definer set search_path = public as $$
  update nx_social_notifications set read_at = now() where user_id = auth.uid() and read_at is null;
$$;

-- ---------------------------------------------------------------- username lookup for sign-in
-- Used only by the nx-login Edge Function (service role); never exposed to clients.
create or replace function public.nx_login_email(p_username text)
returns text language sql stable security definer set search_path = public, auth as $$
  select u.email from auth.users u join public.profiles p on p.id = u.id
  where lower(p.username) = lower(trim(p_username)) limit 1;
$$;
revoke all on function public.nx_login_email(text) from public, anon, authenticated;
grant execute on function public.nx_login_email(text) to service_role;

-- ---------------------------------------------------------------- grants
do $$
declare f text;
begin
  foreach f in array array[
    'nx_social_profile(uuid)', 'nx_social_save_profile(text,text,text,text,text)',
    'nx_social_update_profile(text,text,text,text,text,text)', 'nx_social_set_avatar(text)', 'nx_social_set_privacy(boolean)',
    'nx_social_toggle_follow(uuid,boolean)', 'nx_social_respond_follow_request(uuid,boolean)', 'nx_social_remove_follower(uuid)',
    'nx_social_follow_requests(int,int)', 'nx_social_follow_list(uuid,text,int,int)', 'nx_social_search(text,int)',
    'nx_social_user_posts(uuid,int,timestamptz)', 'nx_social_open_dm(uuid)', 'nx_social_send_dm(uuid,text)',
    'nx_social_dm_messages(uuid,int,timestamptz)', 'nx_social_mark_chat_read(uuid)', 'nx_social_notifications_list(int)',
    'nx_social_mark_notification(uuid)', 'nx_social_mark_all_notifications()', 'nx_social_can_view(uuid)']
  loop
    execute format('revoke all on function public.%s from public, anon', f);
    execute format('grant execute on function public.%s to authenticated', f);
  end loop;
end $$;

-- ---------------------------------------------------------------- realtime
do $$
begin
  if not exists (select 1 from pg_publication_tables where pubname = 'supabase_realtime' and tablename = 'nx_social_notifications') then
    alter publication supabase_realtime add table public.nx_social_notifications;
  end if;
end $$;

commit;
