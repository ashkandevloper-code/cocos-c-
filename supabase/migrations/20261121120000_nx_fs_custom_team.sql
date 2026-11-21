-- Finger Soccer: one personal photo team per paid account (auth/main project).
-- Apply AFTER 20261120000000_nx_gems_shop and 20261121000000_nx_pro_economy_hardening.
-- No client supplies a price. nx_buy_with_gems atomically charges the catalog's
-- fixed 1000 gems once and adds 'fs_team:custom' to the protected inventory.
-- Separate public bucket: only the paid owner may upload/replace their one PNG.
-- Public read makes the cosmetic photo visible to opponents when online support
-- is authorized; do not put private pictures in this bucket.

insert into public.nx_catalog(item_id, price, currency, kind, name, active)
values ('fs_team:custom', 1000, 'gems', 'team', 'Custom photo team', true)
on conflict (item_id) do update set price = 1000, currency = 'gems', kind = 'team',
  name = 'Custom photo team', updated_at = now();
-- A future admin/catalog edit cannot silently sell this entitlement for less
-- (or switch it to coin purchases). An admin may still deactivate the item.
do $$ begin
  alter table public.nx_catalog add constraint nx_fs_custom_price_fixed
    check (item_id <> 'fs_team:custom' or (price = 1000 and currency = 'gems')) not valid;
exception when duplicate_object then null; end $$;
alter table public.nx_catalog validate constraint nx_fs_custom_price_fixed;

insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values ('nx-fs-teams', 'nx-fs-teams', true, 1048576, array['image/png'])
on conflict (id) do update set public = true, file_size_limit = 1048576,
  allowed_mime_types = array['image/png'];

-- The hardening migration's BEFORE UPDATE trigger prevents clients from
-- manufacturing data.inv. Use that protected entitlement in every write policy.
drop policy if exists nx_fs_team_read on storage.objects;
create policy nx_fs_team_read on storage.objects for select
  using (bucket_id = 'nx-fs-teams');

drop policy if exists nx_fs_team_insert on storage.objects;
create policy nx_fs_team_insert on storage.objects for insert to authenticated
  with check (
    bucket_id = 'nx-fs-teams'
    and name = auth.uid()::text || '/team.png'
    and exists (select 1 from public.nx_profiles p where p.id = auth.uid()
      and coalesce(p.data -> 'inv', '[]'::jsonb) ? 'fs_team:custom')
  );

drop policy if exists nx_fs_team_update on storage.objects;
create policy nx_fs_team_update on storage.objects for update to authenticated
  using (bucket_id = 'nx-fs-teams' and owner = auth.uid())
  with check (
    bucket_id = 'nx-fs-teams'
    and owner = auth.uid()
    and name = auth.uid()::text || '/team.png'
    and exists (select 1 from public.nx_profiles p where p.id = auth.uid()
      and coalesce(p.data -> 'inv', '[]'::jsonb) ? 'fs_team:custom')
  );

-- The owner can remove an old photo even after an admin revokes the entitlement.
drop policy if exists nx_fs_team_delete on storage.objects;
create policy nx_fs_team_delete on storage.objects for delete to authenticated
  using (bucket_id = 'nx-fs-teams' and owner = auth.uid()
    and name = auth.uid()::text || '/team.png');

-- Never trust a locally cached inventory or a guessed public URL when allowing
-- selection. This read-only RPC reports the caller's own entitlement and the
-- Storage object's revision (for image cache busting). It cannot grant items.
create or replace function public.nx_fs_custom_status() returns jsonb
language plpgsql security definer set search_path = public, storage as $$
declare me uuid := auth.uid(); owned boolean; rev bigint;
begin
  if me is null then return jsonb_build_object('ok', false, 'owned', false, 'photo', false, 'version', 0); end if;
  select coalesce(p.data -> 'inv', '[]'::jsonb) ? 'fs_team:custom'
    into owned from public.nx_profiles p where p.id = me;
  if not coalesce(owned, false) then
    return jsonb_build_object('ok', true, 'owned', false, 'photo', false, 'version', 0);
  end if;
  select (extract(epoch from o.updated_at) * 1000)::bigint into rev
    from storage.objects o where o.bucket_id = 'nx-fs-teams' and o.name = me::text || '/team.png';
  return jsonb_build_object('ok', true, 'owned', true,
    'photo', rev is not null, 'version', coalesce(rev, 0));
end $$;
revoke all on function public.nx_fs_custom_status() from public, anon;
grant execute on function public.nx_fs_custom_status() to authenticated, service_role;
