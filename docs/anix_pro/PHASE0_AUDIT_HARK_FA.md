# NEXORA PRO — Phase 0 Audit (Hark)
تاریخ: 2026-11-21 — سورس: nexora-universe-work-fs-v7.zip

## خط مبنا (اجراشده واقعی)
- Flutter 3.50.0 / Dart 3.14.0 (همان نسخه CI)
- `flutter pub get`: موفق (79 پکیج نسخه جدیدتر ناسازگار دارند)
- `flutter analyze`: بدون هیچ خطا/هشدار
- `flutter test`: 664 تست، همه پاس
- Build اندروید: انجام نشد (پوشه android/ در مخزن نیست و در CI با flutter create ساخته می‌شود؛ محیط ما arm64 است)
- حجم کد: ~110k خط Dart؛ 21 + 14 migration (~10.6k خط SQL)؛ 3 Edge Function

## P0 — آسیب‌پذیری‌های اقتصاد و رقابت (یافته‌های خط مبنای تاریخی)
1. nx_award (core): کلاینت خودش p_xp و p_coins و p_key را می‌فرستد. سقف فقط برای XP روزانه است؛ سکه هر کلید جدید تا 300 (یا 1000 با event=achievement.*) → ساخت نامحدود Coin. اپ خودش با 0 صدا می‌زند، پس حذف/محدودکردن سمت سرور به کلاینت آسیبی نمی‌زند.
2. nx_purchase: اگر آیتم در nx_catalog نباشد، قیمت ارسالی کلاینت (1..5000) پذیرفته و آیتم به inventory اضافه می‌شود → خرید هر آیتم ناشناخته با 1 سکه / تزریق آیتم دلخواه.
3. nx_import_local: یک‌بار تا 60000 XP و 20000 Coin از عدد ارسالی کلاینت.
4. nx_finish_match (games): هر بازیکن حاضر در مسابقه می‌تواند برنده و امتیاز را خودش اعلام کند؛ اولین فراخوان برنده است → جعل نتیجه، ELO، XP و Coin. فیزیک آنلاین host-authoritative روی گوشی است.
5. nx_arcade_submit: امتیاز آرکید را کلاینت گزارش می‌کند؛ فقط بازه و نرخ معقول چک می‌شود.
6. Rating: ثابت +16/-12 است، نه ELO واقعی (README ادعای ELO دارد).
7. جدول profiles روی پروژه Core از قبل وجود داشته و policy/grant آن در Migrationها نیست؛ باید روی دیتابیس زنده چک شود که UPDATE مستقیم coins/gems/xp از کلاینت ممکن نباشد (revoke ستونی در Postgres در برابر grant جدولی بی‌اثر است).

## P0 — Build و امضا
- سیاست قبلی تولید و Cache کردن `debug.keystore` حذف شد؛ Workflow انتشار در صورت نبود `NEXORA_KEYSTORE_B64` متوقف می‌شود و دیگر کلید جدید تولید نمی‌کند. برای انتشار عمومی/Play هنوز باید Secret به یک Release/Upload Key واقعی متصل، تنظیمات Gradle با آن تأیید، و Play App Signing/AAB در یک Run موفق اثبات شود.
- برند: ANIX در label/README/Release، NEXORA در طراحی؛ package id com.nexora.nexora.
- anon keyها در workflow و fallback داخل nx-call-token هاردکد هستند (Secret نیستند، ولی باید به Variables منتقل شوند).

## موارد سالم
- LiveKit: توکن فقط بعد از بررسی عضویت، TTL پانزده دقیقه، نقش speaker/listener از سرور. (Mute لحظه‌ای فقط با صدور توکن جدید/API سرور LiveKit مؤثر است — بررسی شود.)
- Tournament engine، توابع داخلی _nx_* از public/anon/authenticated revoke شده‌اند.
- nx_buy_with_gems، nx_daily_gems: قیمت از کاتالوگ سرور، ledger تراکنش.
- Analyze تمیز و پوشش تست واحد خوب.

## وضعیت اصلاحات و کار باقی‌مانده
- برای موارد ۱ تا ۶، Migrationهای `20261121000000_nx_pro_economy_hardening.sql` و `20261121000000_g_pro_result_integrity.sql` در سورس اضافه شده‌اند: قوانین پاداش از سرور، خرید از کاتالوگ، Import محدود، حفاظت ستون‌های اقتصاد، اجماع گزارش نتیجه، تشخیص dispute و Elo.
- این اصلاحات هنوز بدون اجرای موفق روی Staging و Run سبز GitHub Actions، به‌عنوان رفع نهایی Production تأیید نمی‌شوند.
- مورد ۷ همچنان نیازمند بررسی Grant/Policy روی دیتابیس واقعی است.
- نتیجه‌ی Migrationها برای بازی‌های نوبتی هنوز جایگزین اعتبارسنجی کامل حرکت در سرور نیست.

## پیشنهاد اصلاح (Phase 1 / 3)
- nx_award: پذیرش فقط event کدهای شناخته‌شده از جدول سرور (nx_reward_rules) با مبلغ تعیین‌شده سرور؛ پارامتر مبلغ حذف.
- nx_purchase: رد هر آیتمی که در کاتالوگ فعال نیست.
- nx_import_local: غیرفعال یا سقف بسیار کم.
- نتیجه مسابقه: نوبتی‌ها (Backgammon, Ludo, UNO, Snakes, Quiz, Mafia) → اعتبارسنجی حرکت در سرور و تعیین برنده توسط سرور. فیزیکی‌ها → گزارش هر دو طرف + state_hash مشترک؛ اختلاف = dispute/بدون پاداش.
- ELO واقعی با K-factor.
- امضای Production + AAB + جداسازی Debug/Release.

## نیاز از مالک پروژه
1. دسترسی به مخزن GitHub (Ashimashi74/nexora-universe) یا تأیید اینکه خروجی به‌صورت patch/zip تحویل شود.
2. یک پروژه Supabase Staging (نه Production) برای اجرای Migrationها.
3. تصمیم برند نهایی: NEXORA یا ANIX.
