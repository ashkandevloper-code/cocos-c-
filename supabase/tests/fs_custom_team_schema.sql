-- Run on STAGING after main-project migrations (psql -v ON_ERROR_STOP=1).
-- Read-only assertions; this does not simulate a purchase or upload.
begin;
do $$
declare r record;
begin
  select price, currency, active into r from public.nx_catalog where item_id = 'fs_team:custom';
  if not found or r.price is distinct from 1000 or r.currency is distinct from 'gems' or not r.active then
    raise exception 'custom photo team must be on sale for exactly 1000 gems';
  end if;
  if not exists (select 1 from pg_constraint where conname = 'nx_fs_custom_price_fixed' and convalidated) then
    raise exception 'fixed custom team price constraint missing';
  end if;
  if not exists (select 1 from storage.buckets where id = 'nx-fs-teams' and public
    and file_size_limit = 1048576 and allowed_mime_types = array['image/png']) then
    raise exception 'custom photo bucket is not configured';
  end if;
  if (select count(*) from pg_policies where schemaname = 'storage' and tablename = 'objects'
    and policyname in ('nx_fs_team_read', 'nx_fs_team_insert', 'nx_fs_team_update', 'nx_fs_team_delete')) <> 4 then
    raise exception 'a paid-owner storage policy is missing';
  end if;
  if not exists (select 1 from pg_proc where oid = 'public.nx_fs_custom_status()'::regprocedure) then
    raise exception 'custom team status RPC is missing';
  end if;
end $$;
rollback;
