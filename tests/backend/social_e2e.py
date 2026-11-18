#!/usr/bin/env python3
"""End-to-end test of the NEXORA social backend against the live Supabase project.
Creates two real accounts through public sign-up (anon key), runs the scenarios, prints PASS/FAIL per step.
Usage: SUPABASE_URL=... SUPABASE_ANON_KEY=... python3 social_e2e.py   (prints the created user ids for cleanup)"""
import io, json, os, sys, time, urllib.request, urllib.error, struct, zlib

URL = os.environ["SUPABASE_URL"].rstrip("/")
ANON = os.environ["SUPABASE_ANON_KEY"]
results = []

def req(method, path, body=None, token=None, headers=None, raw=None, ctype="application/json"):
    h = {"apikey": ANON, "Content-Type": ctype}
    if token: h["Authorization"] = "Bearer " + token
    if headers: h.update(headers)
    data = raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
    r = urllib.request.Request(URL + path, data=data, method=method, headers=h)
    try:
        with urllib.request.urlopen(r, timeout=30) as resp:
            t = resp.read().decode()
            return resp.status, (json.loads(t) if t else None)
    except urllib.error.HTTPError as e:
        t = e.read().decode()
        try: return e.code, json.loads(t)
        except Exception: return e.code, t

def check(name, cond, info=""):
    results.append((name, bool(cond)))
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {info}"))

def rpc(tok, fn, **params): return req("POST", f"/rest/v1/rpc/{fn}", params, tok)

def png(w=8, h=8, rgb=(53, 214, 255)):
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")

ts = str(int(time.time()))[-7:]
pw = "Nx-test-" + ts + "!"
users = {}
for k in ("a", "b"):
    email = f"nx.e2e.{k}.{ts}@nexora-e2e.dev"
    uname = f"e2e_{k}_{ts}"
    s, r = req("POST", "/auth/v1/signup", {"email": email, "password": pw, "data": {"username": uname}})
    ok = s == 200 and isinstance(r, dict) and r.get("access_token")
    check(f"1-2 sign-up user {k.upper()}", ok, (s, r))
    if not ok: sys.exit(1)
    users[k] = {"email": email, "uname": uname, "id": r["user"]["id"], "tok": r["access_token"], "rt": r["refresh_token"]}
A, B = users["a"], users["b"]
print("CREATED", A["id"], B["id"])

# profiles + username
for u, k in ((A, "A"), (B, "B")):
    s, r = rpc(u["tok"], "nx_social_save_profile", p_display_name=f"Tester {k}", p_username=u["uname"], p_bio=f"bio {k}",
               p_website="nexora.app", p_location="Tehran")
    check(f"profile save {k}", s == 200 and r["display_name"] == f"Tester {k}" and r["bio"] == f"bio {k}", (s, r))
s, r = rpc(A["tok"], "nx_social_save_profile", p_display_name="x", p_username=B["uname"], p_bio="", p_website="", p_location="")
check("unique username enforced", s >= 400 and "username_taken" in json.dumps(r), (s, r))

# 3) avatars
for u, k, col in ((A, "A", (255, 79, 163)), (B, "B", (61, 220, 151))):
    path = f"{u['id']}/av_{ts}.png"
    s, r = req("POST", f"/storage/v1/object/avatars/{path}", token=u["tok"], raw=png(rgb=col), ctype="image/png",
               headers={"x-upsert": "true"})
    check(f"3 upload avatar {k}", s == 200, (s, r))
    s, r = rpc(u["tok"], "nx_social_set_avatar", p_path=path)
    check(f"3 set avatar {k}", s == 200 and r["profile"]["avatar_url"].endswith(path), (s, r))
    u["avatar"] = r["profile"]["avatar_url"] if s == 200 else None
    st = urllib.request.urlopen(u["avatar"]).status if u["avatar"] else 0
    check(f"3 avatar public URL loads {k}", st == 200, st)
s, r = req("POST", f"/storage/v1/object/avatars/{B['id']}/evil.png", token=A["tok"], raw=png(), ctype="image/png")
check("14 A cannot upload into B's avatar folder", s >= 400, (s, r))
s, r = rpc(A["tok"], "nx_social_set_avatar", p_path=f"{B['id']}/av_{ts}.png")
check("14 A cannot point avatar at B's file", s >= 400, (s, r))
s, r = req("POST", f"/storage/v1/object/avatars/{A['id']}/x.txt", token=A["tok"], raw=b"hello", ctype="text/plain")
check("upload rejects non-image type", s >= 400, (s, r))

# 4) A opens B's profile
s, r = rpc(A["tok"], "nx_social_profile", p_user=B["id"])
check("4 A sees B's profile (B's own data)", s == 200 and r["id"] == B["id"] and r["username"] == B["uname"]
      and r["avatar_url"] == B["avatar"] and not r["is_me"] and r["followers"] == 0, (s, r))

# self follow / follow
s, r = rpc(A["tok"], "nx_social_toggle_follow", p_target=A["id"], p_follow=True)
check("cannot follow self", s >= 400, (s, r))
s, r = rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=True)
check("5 A follows B", s == 200 and r["is_following"] and r["followers"] == 1, (s, r))
s, r = rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=True)
check("duplicate follow is idempotent", s == 200 and r["followers"] == 1, (s, r))
s, r = req("POST", "/rest/v1/follows", {"follower": A["id"], "followee": B["id"]}, A["tok"])
check("duplicate follow row rejected by DB", s >= 400, (s, r))
s, ra = rpc(A["tok"], "nx_social_profile", p_user=A["id"])
s2, rb = rpc(B["tok"], "nx_social_profile", p_user=B["id"])
check("6 counters A.following=1, B.followers=1", ra["following"] == 1 and rb["followers"] == 1, (ra, rb))
s, r = rpc(B["tok"], "nx_social_follow_list", p_user=B["id"], p_kind="followers")
check("followers list of B contains A", s == 200 and [x["id"] for x in r] == [A["id"]] and r[0]["avatar_url"] == A["avatar"], (s, r))
s, r = rpc(A["tok"], "nx_social_follow_list", p_user=A["id"], p_kind="following")
check("following list of A contains B", s == 200 and [x["id"] for x in r] == [B["id"]], (s, r))
s, r = rpc(B["tok"], "nx_social_notifications_list")
check("10 B got a 'follow' notification", s == 200 and any(n["kind"] == "follow" and n["actor"]["id"] == A["id"] for n in r["items"]), (s, r))

# 7) persists across a new session (fresh sign-in)
s, r = req("POST", "/auth/v1/token?grant_type=password", {"email": A["email"], "password": pw})
check("7 A signs in again", s == 200, (s, r))
A["tok"], A["rt"] = r["access_token"], r["refresh_token"]
s, r = rpc(A["tok"], "nx_social_profile", p_user=B["id"])
check("7 follow state survives re-login", s == 200 and r["is_following"], (s, r))

# 8-9) unfollow
s, r = rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=False)
check("8 A unfollows B", s == 200 and not r["is_following"] and r["followers"] == 0, (s, r))
s, ra = rpc(A["tok"], "nx_social_profile", p_user=A["id"])
check("9 A.following back to 0", ra["following"] == 0, ra)

# private account + follow requests
s, r = rpc(B["tok"], "nx_social_set_privacy", p_private=True)
check("B goes private", s == 200 and r["is_private"], (s, r))
s, r = req("POST", "/rest/v1/follows", {"follower": A["id"], "followee": B["id"]}, A["tok"])
check("direct REST follow of private account blocked", s >= 400, (s, r))
s, r = rpc(A["tok"], "nx_social_follow_list", p_user=B["id"], p_kind="followers")
check("private follower list hidden from non-follower", s >= 400 and "private_account" in json.dumps(r), (s, r))
s, r = rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=True)
check("follow private -> pending", s == 200 and r["is_pending"] and not r["is_following"] and not r["can_view"], (s, r))
s, r = rpc(B["tok"], "nx_social_follow_requests")
check("B sees A's request", s == 200 and [x["id"] for x in r] == [A["id"]], (s, r))
s, r = rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=False)
check("A cancels request", s == 200 and not r["is_pending"], (s, r))
s, r = rpc(B["tok"], "nx_social_follow_requests")
check("cancelled request gone", s == 200 and r == [], (s, r))
rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=True)
s, r = rpc(B["tok"], "nx_social_respond_follow_request", p_requester=A["id"], p_accept=True)
check("B accepts request", s == 200 and r["followers"] == 1, (s, r))
s, r = rpc(A["tok"], "nx_social_profile", p_user=B["id"])
check("A now follows private B and can view", r["is_following"] and r["can_view"], r)
s, r = rpc(A["tok"], "nx_social_notifications_list")
check("A got 'follow_accepted'", any(n["kind"] == "follow_accepted" for n in r["items"]), r)
rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=False)
rpc(A["tok"], "nx_social_toggle_follow", p_target=B["id"], p_follow=True)
s, r = rpc(B["tok"], "nx_social_respond_follow_request", p_requester=A["id"], p_accept=False)
check("B rejects request", s == 200 and r["followers"] == 0, (s, r))
rpc(B["tok"], "nx_social_set_privacy", p_private=False)

# 10-11) messaging from profile
s, r = rpc(A["tok"], "nx_social_open_dm", p_user=B["id"])
check("10 A opens DM with B", s == 200 and r["chat_id"] and r["peer"]["id"] == B["id"], (s, r))
chat = r["chat_id"]
s, r = rpc(A["tok"], "nx_social_open_dm", p_user=B["id"])
check("reopening DM reuses the same conversation", s == 200 and r["chat_id"] == chat, (s, r))
s, r = rpc(B["tok"], "nx_social_open_dm", p_user=A["id"])
check("B opening DM with A gets the same conversation", s == 200 and r["chat_id"] == chat, (s, r))
s, m1 = rpc(A["tok"], "nx_social_send_dm", p_chat=chat, p_text="salam B 👋")
time.sleep(0.05)
s2, m2 = rpc(A["tok"], "nx_social_send_dm", p_chat=chat, p_text="second")
check("10 A sends 2 messages", s == 200 and s2 == 200 and m1["author"] == A["id"], (s, m1, s2, m2))
s, r = rpc(B["tok"], "nx_social_dm_messages", p_chat=chat)
texts = [x["text"] for x in r["messages"]] if s == 200 else []
check("11 B receives messages in time order", texts == ["salam B 👋", "second"], (s, r))
check("timestamps are UTC ISO", s == 200 and r["messages"][0]["created_at"].endswith("+00:00"), r)
s, r = rpc(A["tok"], "nx_social_dm_messages", p_chat=chat)
check("not seen before B reads", s == 200 and (r["peer_last_read"] is None or r["peer_last_read"] < m2["created_at"]), r)
rpc(B["tok"], "nx_social_mark_chat_read", p_chat=chat)
s, r = rpc(A["tok"], "nx_social_dm_messages", p_chat=chat)
check("seen after B reads", s == 200 and r["peer_last_read"] >= m2["created_at"], r)
s, r = rpc(B["tok"], "nx_social_notifications_list")
check("B has one (read) message notification", s == 200 and sum(1 for n in r["items"] if n["kind"] == "message") == 1, r)
s, r = req("GET", f"/rest/v1/nx_messages?chat_id=eq.{chat}&select=id", token=None)
check("anonymous cannot read DMs", s >= 400 or r == [], (s, r))

# 12-13) sign out + sign in, data intact
for u, k in ((A, "A"), (B, "B")):
    s, r = req("POST", "/auth/v1/logout", {}, u["tok"])
    check(f"12 {k} signs out", s in (200, 204), (s, r))
    s, r = req("POST", "/auth/v1/token?grant_type=refresh_token", {"refresh_token": u["rt"]})
    check(f"12 {k} refresh token revoked after sign-out", s >= 400, (s, r))
s, r = req("POST", "/auth/v1/token?grant_type=password", {"email": B["email"], "password": pw})
B["tok"] = r["access_token"]
s, r = rpc(B["tok"], "nx_social_dm_messages", p_chat=chat)
check("13 messages persist after re-login", s == 200 and len(r["messages"]) == 2, r)
s, r = rpc(B["tok"], "nx_social_profile", p_user=B["id"])
check("13 profile + avatar persist", s == 200 and r["avatar_url"] == B["avatar"] and r["bio"] == "bio B", r)

# username login through the nx-login Edge Function
s, r = req("POST", "/functions/v1/nx-login", {"username": A["uname"], "password": pw})
check("login with username (nx-login)", s == 200 and r.get("user", {}).get("id") == A["id"], (s, r))
A["tok"] = r.get("access_token", A["tok"])
s, r = req("POST", "/functions/v1/nx-login", {"username": A["uname"], "password": "wrong-password"})
check("username login with wrong password fails", s >= 400, (s, r))
s, r = req("POST", "/auth/v1/token?grant_type=password", {"email": A["email"], "password": "wrong-password"})
check("e-mail login with wrong password fails", s >= 400, (s, r))

# 14) cannot change other people's data
s, r = req("PATCH", f"/rest/v1/profiles?id=eq.{B['id']}", {"bio": "hacked"}, A["tok"], headers={"Prefer": "return=representation"})
check("14 A cannot edit B's profile via REST", s >= 400 or r == [], (s, r))
s, r = rpc(B["tok"], "nx_social_profile", p_user=B["id"])
check("14 B's bio unchanged", r["bio"] == "bio B", r)
rpc(B["tok"], "nx_social_toggle_follow", p_target=A["id"], p_follow=True)
req("DELETE", f"/rest/v1/follows?follower=eq.{B['id']}", None, A["tok"])
s, r = rpc(B["tok"], "nx_social_profile", p_user=B["id"])
check("14 A cannot delete B's follow rows", r["following"] == 1, r)
s, r = rpc(A["tok"], "nx_social_dm_messages", p_chat="00000000-0000-0000-0000-000000000000")
check("14 non-member cannot read a chat", s >= 400, (s, r))

# password change
newpw = pw + "x"
s, r = req("PUT", "/auth/v1/user", {"password": newpw}, A["tok"])
check("change password", s == 200, (s, r))
s, r = req("POST", "/auth/v1/token?grant_type=password", {"email": A["email"], "password": pw})
check("old password no longer works", s >= 400, (s, r))
s, r = req("POST", "/auth/v1/token?grant_type=password", {"email": A["email"], "password": newpw})
check("new password works", s == 200, (s, r))

# rate limit on DMs
A["tok"] = r["access_token"]
codes = [rpc(A["tok"], "nx_social_send_dm", p_chat=chat, p_text=f"spam {i}")[0] for i in range(31)]
check("DM spam is rate-limited (30/min)", codes[-1] >= 400 and codes[0] == 200, codes)

# cleanup: remove avatar objects (the accounts are deleted by the caller with the printed ids)
for u in (A, B):
    s, r = req("POST", "/auth/v1/token?grant_type=password", {"email": u["email"], "password": newpw if u is A else pw})
    if s == 200:
        req("DELETE", "/storage/v1/object/avatars", {"prefixes": [f"{u['id']}/av_{ts}.png"]}, r["access_token"])

passed = sum(1 for _, ok in results if ok)
print(f"\n{passed}/{len(results)} passed")
print("CLEANUP", A["id"], B["id"])
sys.exit(0 if passed == len(results) else 1)
