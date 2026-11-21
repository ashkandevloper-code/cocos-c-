import 'package:flutter/material.dart';

import '../../core/assets/app_assets.dart';
import '../../games/game_registry.dart';

/// ANIX PRO information architecture (Station Luxe). Every item opens a real, existing screen or a real in-app action
/// (no "coming soon" tiles): [route] is a go_router location, [action] a named in-app action.
///
///   Dock:  Home · Social · PLAY · Compete · Profile
///   Games    Sports · Table · Cards & party · Online (rooms + voice)
///   Social   Messages · Feed & stories · Communities
///   Compete  Tournaments · Join by code · Invitations · Create · Rankings
///   Rewards  Daily · Missions · Achievements (only here, not in the profile)
///   Shop     Teams & gems · Custom team · Cosmetics · History
///   Oracle   ORACLE AI
///   Profile  Profile · Account · Settings · Admin (staff only)
class LuxeItem {
  const LuxeItem({
    required this.fa,
    required this.en,
    required this.tab,
    this.icon,
    this.image,
    this.route,
    this.action,
    this.subFa,
    this.subEn,
    this.staffOnly = false,
    this.online = false,
  });
  final String fa;
  final String en;
  final String? subFa;
  final String? subEn;
  final int tab;
  final String? icon; // assets/luxe/icons/<icon>.webp
  final String? image; // full asset path (game pedestals)
  final String? route;
  final String? action; // 'daily' = claim today's reward
  final bool staffOnly;
  final bool online;
}

class LuxeWorld {
  const LuxeWorld({
    required this.k,
    required this.route,
    required this.fa,
    required this.en,
    required this.icon,
    required this.color,
    required this.tagFa,
    required this.tagEn,
    required this.tabsFa,
    required this.tabsEn,
    required this.items,
  });
  final String k;

  /// The canonical location of this station (dock / hub carousel target).
  final String route;
  final String fa;
  final String en;
  final String icon;
  final Color color;
  final String tagFa;
  final String tagEn;
  final List<String> tabsFa;
  final List<String> tabsEn;
  final List<LuxeItem> items;
}

/// Games category of a registered game: 0 sports & action, 1 table, 2 cards & party.
int luxeGameCategory(GameInfo g) {
  const Map<String, int> fixed = <String, int>{
    'finger_soccer': 0,
    'basketball_battle': 0,
    'mini_golf': 0,
    'kart_racing': 0,
    'obstacle_race': 0,
    'bomb_arena': 0,
    'billiards': 1,
    'pool8': 1,
    'carrom': 1,
    'backgammon': 1,
    'ludo': 1,
    'snakes_ladders': 1,
    'uno': 2,
    'mafia': 2,
    'spy_mafia': 2,
    'quiz': 2,
  };
  final int? f = fixed[g.id];
  if (f != null) return f;
  return switch (g.group) { 2 => 1, 3 => 2, _ => 0 };
}

List<LuxeItem> _gameItems() {
  final List<LuxeItem> out = <LuxeItem>[
    for (final GameInfo g in kGames)
      LuxeItem(
          fa: g.nameFa,
          en: g.nameEn,
          subFa: g.taglineFa,
          subEn: g.taglineEn,
          tab: luxeGameCategory(g),
          image: AppAssets.pedestal(g.id),
          route: '/arcade/${g.id}',
          online: g.supportsOnline),
  ];
  out.addAll(const <LuxeItem>[
    LuxeItem(
        fa: 'بازی سریع آنلاین',
        en: 'Quick online match',
        subFa: 'حریف هم‌سطح در چند ثانیه',
        subEn: 'An opponent in seconds',
        tab: 3,
        icon: 'r4c6',
        route: '/rooms?tab=quick',
        online: true),
    LuxeItem(
        fa: 'روم‌های بازی',
        en: 'Game rooms',
        subFa: 'روم بساز یا با کد وارد شو',
        subEn: 'Create a room or join by code',
        tab: 3,
        icon: 'r4c5',
        route: '/rooms?tab=rooms',
        online: true),
    LuxeItem(
        fa: 'روم‌های صوتی',
        en: 'Voice rooms',
        subFa: 'گفتگوی زنده با بازیکن‌ها',
        subEn: 'Live talk with players',
        tab: 3,
        icon: 'r3c1',
        route: '/rooms?tab=talk',
        online: true),
  ]);
  return out;
}

List<LuxeWorld> buildLuxeWorlds() => <LuxeWorld>[
      LuxeWorld(
        k: 'games',
        route: '/play',
        fa: 'بازی‌ها',
        en: 'GAMES',
        icon: 'r4c5',
        color: const Color(0xFFE2C27A),
        tagFa: 'ورزشی، میزی، کارتی و آنلاین',
        tagEn: 'Sports, table, cards and online',
        tabsFa: const <String>['ورزشی', 'میزی', 'کارتی و گروهی', 'آنلاین و صوتی'],
        tabsEn: const <String>['Sports', 'Table', 'Cards & party', 'Online & voice'],
        items: _gameItems(),
      ),
      const LuxeWorld(
        k: 'social',
        route: '/social',
        fa: 'اجتماعی',
        en: 'SOCIAL',
        icon: 'r3c3',
        color: Color(0xFFA8D8C0),
        tagFa: 'پیام، پست و جامعه‌ها',
        tagEn: 'Messages, posts and communities',
        tabsFa: <String>['پیام‌ها', 'پست و استوری', 'جامعه‌ها'],
        tabsEn: <String>['Messages', 'Feed & stories', 'Communities'],
        items: <LuxeItem>[
          LuxeItem(
              fa: 'گفتگوها',
              en: 'Chats',
              subFa: 'پیام خصوصی، گروه و کانال',
              subEn: 'Direct, groups and channels',
              tab: 0,
              icon: 'r3c3',
              route: '/social/messages'),
          LuxeItem(
              fa: 'دوستان',
              en: 'Friends',
              subFa: 'دنبال‌کننده‌ها و درخواست‌ها',
              subEn: 'Followers and requests',
              tab: 0,
              icon: 'r3c4',
              route: '/social/messages?tab=friends'),
          LuxeItem(
              fa: 'پست و استوری',
              en: 'Feed & stories',
              subFa: 'لحظه‌هایت را به اشتراک بگذار',
              subEn: 'Share your moments',
              tab: 1,
              icon: 'r5c3',
              route: '/social/feed'),
          LuxeItem(
              fa: 'جامعه‌ها',
              en: 'Communities',
              subFa: 'باشگاه‌ها و رویدادهای گروهی',
              subEn: 'Clubs and group events',
              tab: 2,
              icon: 'r3c4',
              route: '/social/communities'),
        ],
      ),
      const LuxeWorld(
        k: 'compete',
        route: '/compete',
        fa: 'رقابت',
        en: 'COMPETE',
        icon: 'r2c5',
        color: Color(0xFFE2C27A),
        tagFa: 'تورنمنت، دعوت و رده‌بندی',
        tagEn: 'Tournaments, invites and ranks',
        tabsFa: <String>['تورنمنت‌ها', 'رده‌بندی'],
        tabsEn: <String>['Tournaments', 'Rankings'],
        items: <LuxeItem>[
          LuxeItem(
              fa: 'تورنمنت‌ها',
              en: 'Tournaments',
              subFa: 'حذفی، لیگ و سوئیسی با جایزه',
              subEn: 'Knockout, league and swiss',
              tab: 0,
              icon: 'r2c5',
              route: '/tournaments'),
          LuxeItem(
              fa: 'ورود با کد', en: 'Join by code', subFa: 'کد دعوت را وارد کن', subEn: 'Enter an invite code', tab: 0, icon: 'r4c6', route: '/tournaments/join'),
          LuxeItem(
              fa: 'دعوت‌ها', en: 'Invitations', subFa: 'دعوت‌ها و اعلان‌ها', subEn: 'Invites and notices', tab: 0, icon: 'r5c3', route: '/tournaments/inbox'),
          LuxeItem(
              fa: 'ساخت تورنمنت', en: 'Create', subFa: 'تورنمنت خودت را بساز', subEn: 'Host your own cup', tab: 0, icon: 'r5c4', route: '/tournaments/new'),
          LuxeItem(
              fa: 'رده‌بندی', en: 'Rankings', subFa: 'برترین‌های هر بازی', subEn: 'Top players of every game', tab: 1, icon: 'r2c6', route: '/leaderboard'),
        ],
      ),
      const LuxeWorld(
        k: 'rewards',
        route: '/rewards',
        fa: 'جایزه‌ها',
        en: 'REWARDS',
        icon: 'r3c6',
        color: Color(0xFFE8B7A0),
        tagFa: 'روزانه، ماموریت و دستاورد',
        tagEn: 'Daily, missions and achievements',
        tabsFa: <String>['جایزه‌ها'],
        tabsEn: <String>['Rewards'],
        items: <LuxeItem>[
          LuxeItem(fa: 'جایزه‌ی روزانه', en: 'Daily reward', tab: 0, icon: 'r3c6', route: '/rewards?tab=0'),
          LuxeItem(fa: 'ماموریت‌ها', en: 'Missions', tab: 0, icon: 'r5c2', route: '/rewards?tab=1'),
          LuxeItem(fa: 'دستاوردها', en: 'Achievements', tab: 0, icon: 'r4c3', route: '/rewards?tab=2'),
        ],
      ),
      const LuxeWorld(
        k: 'shop',
        route: '/shop',
        fa: 'فروشگاه',
        en: 'SHOP',
        icon: 'r4c1',
        color: Color(0xFFD9B872),
        tagFa: 'تیم، الماس و ظاهر',
        tagEn: 'Teams, gems and style',
        tabsFa: <String>['فروشگاه'],
        tabsEn: <String>['Shop'],
        items: <LuxeItem>[
          LuxeItem(
              fa: 'تیم‌ها و الماس', en: 'Teams & gems', subFa: 'تیم‌های فوتبال با الماس', subEn: 'Football teams for gems', tab: 0, icon: 'r1c3', route: '/shop/teams'),
          LuxeItem(
              fa: 'تیم اختصاصی', en: 'Custom team', subFa: 'نام و لوگوی خودت', subEn: 'Your own name and logo', tab: 0, icon: 'r4c4', route: '/shop/custom'),
          LuxeItem(
              fa: 'ظاهر و آیتم‌ها', en: 'Cosmetics', subFa: 'آواتار، قاب، مهره و تم', subEn: 'Avatar, frames, pucks, themes', tab: 0, icon: 'r4c3', route: '/shop/cosmetics'),
          LuxeItem(fa: 'تاریخچه', en: 'History', subFa: 'خریدها و تراکنش‌ها', subEn: 'Purchases and transactions', tab: 0, icon: 'r3c5', route: '/shop/history'),
        ],
      ),
      const LuxeWorld(
        k: 'oracle',
        route: '/oracle',
        fa: 'اوراکل',
        en: 'ORACLE AI',
        icon: 'r5c2',
        color: Color(0xFFC9B6F0),
        tagFa: 'راهنما، تحلیل و پیشنهاد',
        tagEn: 'Guide, analysis and tips',
        tabsFa: <String>['اوراکل'],
        tabsEn: <String>['ORACLE'],
        items: <LuxeItem>[
          LuxeItem(fa: 'گفتگو با اوراکل', en: 'Talk to ORACLE', tab: 0, icon: 'r5c2', route: '/oracle'),
        ],
      ),
      const LuxeWorld(
        k: 'profile',
        route: '/profile',
        fa: 'پروفایل',
        en: 'PROFILE',
        icon: 'r4c4',
        color: Color(0xFFD4C4A8),
        tagFa: 'هویت، حساب و تنظیمات',
        tagEn: 'Identity, account and settings',
        tabsFa: <String>['پروفایل'],
        tabsEn: <String>['Profile'],
        items: <LuxeItem>[
          LuxeItem(
              fa: 'پروفایل من', en: 'My profile', subFa: 'هویت، کمد، فضا و آمار', subEn: 'Identity, wardrobe, space, stats', tab: 0, icon: 'r4c4', route: '/profile/me'),
          LuxeItem(fa: 'حساب کاربری', en: 'Account', subFa: 'ورود، رمز و امنیت', subEn: 'Sign-in, password, security', tab: 0, icon: 'r3c4', route: '/account'),
          LuxeItem(fa: 'تنظیمات', en: 'Settings', subFa: 'روز و شب، صدا، زبان', subEn: 'Day & night, sound, language', tab: 0, icon: 'r4c2', route: '/settings'),
          LuxeItem(
              fa: 'مدیریت', en: 'Admin', subFa: 'فقط کارکنان', subEn: 'Staff only', tab: 0, icon: 'r4c6', route: '/admin', staffOnly: true),
        ],
      ),
    ];

/// Station by key. Old keys of the previous menus map to their new home.
LuxeWorld? luxeWorld(String k) {
  final String key = switch (k) {
    'chat' => 'social',
    'tournaments' || 'events' => 'compete',
    _ => k,
  };
  for (final LuxeWorld w in buildLuxeWorlds()) {
    if (w.k == key) return w;
  }
  return null;
}
