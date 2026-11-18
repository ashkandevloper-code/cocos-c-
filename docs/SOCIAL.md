# Social network (cocos2d-x client + Supabase)

Everything is stored in Supabase (project `eisstodgncimypdawisy`); the app keeps no social data only on the device.

## Database (`supabase/migrations/20261118300000_nx_social_v2.sql`, `…300100_nx_social_v2_fixes.sql`, applied live)

| Table | Role |
|---|---|
| `auth.users` | accounts; passwords hashed by Supabase Auth (bcrypt), JWT access token (1 h) + rotating refresh token |
| `profiles` | username (unique, case-insensitive), display_name, bio (≤160), website, location, avatar_url / avatar_path, is_private, created_at, updated_at, online_at (last active) |
| `follows` | (follower, followee) primary key = no duplicates, `follower <> followee` check, FKs with cascade, indexes on both sides |
| `nx_follow_requests` | requests to private accounts: pending / accepted / rejected / cancelled, unique (requester, target) |
| `nx_chats` (`kind='direct'`, unique `dm_key`) + `nx_chat_members` + `nx_messages` | one conversation per pair, read position = `nx_chat_members.last_read_at` |
| `nx_social_notifications` | follow, follow_request, follow_accepted, message; `read_at` |
| `nx_rate_events` | server-side rate limits (follow 60/10 min, DM 30/min, profile 20/10 min, avatar 20/10 min) |
| storage bucket `avatars` | public read, 2 MB, image/jpeg·png·webp, users write only `<their uid>/…` |

All times are `timestamptz` (UTC). The client converts to the device time zone with the C library (DST-aware);
Persian UI shows Jalali dates.

### RPCs (SECURITY DEFINER, all check `auth.uid()`)
`nx_social_profile`, `nx_social_save_profile`, `nx_social_set_avatar`, `nx_social_set_privacy`,
`nx_social_toggle_follow`, `nx_social_respond_follow_request`, `nx_social_remove_follower`, `nx_social_follow_requests`,
`nx_social_follow_list`, `nx_social_search`, `nx_social_user_posts`, `nx_social_open_dm`, `nx_social_send_dm`,
`nx_social_dm_messages`, `nx_social_mark_chat_read`, `nx_social_notifications_list`, `nx_social_mark_notification`,
`nx_social_mark_all_notifications`. Private accounts: posts (RLS on `nx_posts`) and follower lists are visible only to
approved followers; a direct REST insert into `follows` cannot bypass a private account.

### Edge Function `nx-login`
Username + password sign-in without exposing anyone's e-mail (resolves the account server-side).

## Client
* `Classes/backend/SocialService.*` — engine-independent API + models (unit tests: `tests/test_social.cpp`).
* `Classes/scenes/SocialScenes.*` — Profile (own/others), Edit profile + photo, Followers / Following / Requests,
  Search, Messages inbox, Direct chat (realtime + 8 s fallback poll, sending / sent / seen), Notifications,
  Post detail, Change password.
* `Classes/ui/Media.*` — cached network images, round avatars with initials placeholder, skeletons.
* Android photo picker (`AppActivity.processPicked`): JPG/PNG/WEBP only, EXIF rotation, square crop, 512 px, JPEG 85.

## Tests
* Host unit tests: `cmake -S tests -B build-host && cmake --build build-host && build-host/nx_tests`.
* Live backend end-to-end (creates two real accounts): `SUPABASE_URL=… SUPABASE_ANON_KEY=… python3 tests/backend/social_e2e.py`,
  then remove them with `tests/backend/cleanup.sql` (SQL editor). 65 checks: sign-up, avatar upload/permissions, follow /
  unfollow / counters, persistence after re-login, private account + requests, DM open/reuse/send/receive/order/seen,
  sign-out revokes the refresh token, username login, password change, cross-account write attempts, rate limit.

## Needs your action (dashboard)
1. **Custom SMTP** (Authentication → Emails → SMTP): the built-in sender only sends ~2 e-mails per hour and Supabase
   blocks template edits without it. Any provider works (Resend, Brevo, Mailgun, Gmail app password).
2. After SMTP is on, put `{{ .Token }}` into the *Magic Link*, *Confirm signup* and *Reset password* templates so
   e-mails also carry a 6-digit code (the app already accepts codes; reset links already open the app via
   `nexora://login-callback` and land on "New password").
