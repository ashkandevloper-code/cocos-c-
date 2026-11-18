-- Sign-up: usernames are unique case-insensitively (profiles_username_lower_key), so the new-user trigger must
-- compare lower-cased too; otherwise "Bob" vs "bob" would abort the whole sign-up.
create or replace function public.handle_new_user()
returns trigger language plpgsql security definer set search_path = public as $$
declare n text;
begin
  n := nullif(btrim(new.raw_user_meta_data->>'username'), '');
  if n is null or char_length(n) < 3 or char_length(n) > 20 then
    n := 'pilot' || substr(replace(new.id::text, '-', ''), 1, 15);
  end if;
  while exists (select 1 from public.profiles where lower(username) = lower(n)) loop
    n := left(n, 14) || lpad((floor(random() * 99999))::bigint::text, 5, '0');
  end loop;
  insert into public.profiles(id, username, is_guest, display_name)
  values (new.id, n, coalesce(new.is_anonymous, false), coalesce(nullif(btrim(new.raw_user_meta_data->>'display_name'), ''), ''));
  return new;
end $$;

-- Inbox rows show the real profile photo (avatar_url) and display name; falls back to the legacy fields.
create or replace function public.nx_my_chats()
returns table(id uuid, kind text, name text, description text, badge_id text, owner uuid, is_public boolean,
              created_at timestamptz, last_msg_at timestamptz, role text, pinned boolean, muted boolean,
              last_read_at timestamptz, unread integer, peer uuid, peer_name text, peer_avatar text,
              peer_online timestamptz, members integer, last jsonb)
language sql stable security definer set search_path = public as $$
  select c.id, c.kind, c.name, c.description, c.badge_id, c.owner, c.is_public, c.created_at, c.last_msg_at,
         m.role, m.pinned, m.muted, m.last_read_at,
         (select count(*)::int from nx_messages x where x.chat_id = c.id and x.created_at > m.last_read_at
            and x.author is distinct from auth.uid() and not x.deleted),
         p.id, p.name, p.avatar, p.online_at,
         (select count(*)::int from nx_chat_members y where y.chat_id = c.id),
         (select to_jsonb(l) from (select * from nx_messages z where z.chat_id = c.id order by z.created_at desc limit 1) l)
  from nx_chat_members m
  join nx_chats c on c.id = m.chat_id
  left join lateral (select pr.id, coalesce(nullif(pr.display_name, ''), pr.username) as name,
                            coalesce(nullif(pr.avatar_url, ''), pr.avatar) as avatar, pr.online_at
                     from nx_chat_members o join profiles pr on pr.id = o.user_id
                     where c.kind = 'direct' and o.chat_id = c.id and o.user_id <> auth.uid() limit 1) p on true
  where m.user_id = auth.uid()
  order by m.pinned desc, c.last_msg_at desc;
$$;
