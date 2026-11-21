# وضعیت اجرای مأموریت ANIX PRO

تاریخ خط مبنا: ۲۱ نوامبر ۲۰۲۶

این سند گزارش واقعی اجرای مأموریت است. وجود UI، Registry، Migration، یا `supportsOnline: true` به‌تنهایی به معنی تکمیل محصول نیست.

## فاز اجراشده در این مرحله

### کنترل دامنه و جلوگیری از Drift

- اسکریپت `tool/validate_anix_scope.py` اضافه شد.
- این اسکریپت در `flutter_quality_gate.yml` قبل از Analyze اجرا می‌شود.
- شناسه‌های Registry باید یکتا باشند.
- ۱۶ بازی ثبت‌شده باید دقیقاً با Entry Moduleهای موجود منطبق باشند.
- برای هر بازی باید dossier مستقل در `docs/anix_pro/game-dossiers/` وجود داشته باشد.
- وجود شواهد تست در `test/games` کنترل می‌شود.
- این کنترل فقط صحت ساختاری را ثابت می‌کند و ادعای Production-ready بودن بازی را نمی‌سازد.

### امضای Android

- CI دیگر در صورت نبود کلید، کلید جدید تولید یا از Cache بازیابی نمی‌کند.
- نبود `NEXORA_KEYSTORE_B64` در Build غیر زمان‌بندی‌شده باعث Fail شدن Workflow می‌شود.
- اثرانگشت عمومی گواهی فقط برای تشخیص هویت کلید در Log چاپ می‌شود.
- هنوز باید Secret با یک Release/Upload Key واقعی و سیاست Play App Signing در Repository تنظیم و در یک Run موفق GitHub Actions اثبات شود.

### مستندات ممیزی

- اشاره‌ی قدیمی به `check.yml` و Fail-Suppression از گزارش ممیزی حذف و با Workflowهای فعلی جایگزین شد.
- وضعیت این مرحله به‌عنوان Baseline/Hardened-in-source ثبت شده، نه Completion Certificate.

## وضعیت بازی‌ها

| دامنه | وضعیت فعلی |
|---|---|
| ۱۶ بازی ثبت‌شده | Source و تست پایه موجود؛ تکمیل Production هر بازی هنوز جداگانه اثبات نشده است |
| بازی‌های roadmap جدید | ثبت playable نشده‌اند؛ نباید در UI به‌عنوان بازی آماده نمایش داده شوند |
| قوانین محلی و Botها | برای بسیاری از بازی‌ها تست‌های Pure Dart موجود است؛ ممیزی کامل قوانین هنوز لازم است |
| Online/Realtime | Migration و قراردادهای مشترک موجود است؛ تست شبکه، reconnect و emulator کامل نیست |
| گرافیک و Assetها | Asset library گسترده است؛ بازبینی بصری همه‌ی اندازه‌ها و مسیرهای UI انجام نشده است |
| Ranking/League | زیرساخت و Migration وجود دارد؛ اعتبارسنجی end-to-end هر بازی و هر فصل اثبات نشده است |

## مواردی که هنوز انجام‌شده محسوب نمی‌شوند

- تست Android Emulator و Navigation واقعی.
- تست Multiplayer با دو یا چند کلاینت مستقل.
- تست قطع و وصل شبکه و Replay/Resync.
- اثبات اجرای Migrationها روی Staging و Production.
- اثبات اصلاحات اقتصاد و نتیجه مسابقه روی پروژه واقعی Supabase.
- تأیید Release Artifact و لینک Run در GitHub Actions.
- امضای نهایی با Release Key واقعی.
- تکمیل بازی‌های جدید roadmap.
- تأیید بصری همه‌ی صفحات و بازی‌ها روی اندازه‌های مختلف نمایشگر.

## ترتیب فازهای بعدی

1. اجرای Quality Gate در GitHub Actions و رفع خطاهای واقعی آن.
2. اجرای Migrationهای hardening روی Staging و افزودن تست‌های RLS/RPC برای اقتصاد و نتیجه مسابقه.
3. اضافه‌کردن `integration_test/` برای ورود، Hub، Setup بازی، Room و پایان مسابقه.
4. افزودن شبیه‌سازی دو کلاینت برای reconnect، نتیجه مسابقه و idempotency.
5. تولید dossier فنی دقیق برای هر یک از ۱۶ بازی شامل Rules, State, AI, Online, Ranking و Known Gaps.
6. فقط پس از عبور فازهای بالا، انتخاب موج اول بازی‌های جدید و ساخت کامل آن‌ها.

## معیار تحویل این فاز

این فاز فقط زمانی موفق تلقی می‌شود که Workflow مربوطه در GitHub Actions سبز شود، گزارش Artifact موجود باشد، و هر موردی که به Staging/Secret/دستگاه نیاز دارد با نتیجه واقعی یا Blocker شفاف ثبت شود. هیچ موردی صرفاً به دلیل وجود فایل یا Migration، Completed اعلام نمی‌شود.
