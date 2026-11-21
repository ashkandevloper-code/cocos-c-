import 'package:flutter/material.dart';

/// ANIX PRO "Station Luxe" design tokens: one palette per mode (night = black / gold, day = ivory / bronze), a single
/// type scale (Vazirmatn for Persian and UI text, Cormorant Garamond for Latin display labels), radii, spacing and
/// motion. Every screen reads these through [LuxPalette.of] / [LuxType], never hard-coded colours.
@immutable
class LuxPalette extends ThemeExtension<LuxPalette> {
  const LuxPalette({
    required this.brightness,
    required this.bg,
    required this.bg2,
    required this.panel,
    required this.panelHi,
    required this.glassTop,
    required this.glassBottom,
    required this.ink,
    required this.mute,
    required this.faint,
    required this.gold,
    required this.gold2,
    required this.gold3,
    required this.onGold,
    required this.line,
    required this.lineStrong,
    required this.shadow,
    required this.success,
    required this.danger,
    required this.live,
    required this.sceneTop,
    required this.sceneBottom,
  });

  final Brightness brightness;
  final Color bg, bg2, panel, panelHi, glassTop, glassBottom;
  final Color ink, mute, faint;
  final Color gold, gold2, gold3, onGold;
  final Color line, lineStrong, shadow;
  final Color success, danger, live;
  final Color sceneTop, sceneBottom;

  bool get isDark => brightness == Brightness.dark;

  /// Night: the reference's black / champagne gold.
  static const LuxPalette night = LuxPalette(
    brightness: Brightness.dark,
    bg: Color(0xFF07080C),
    bg2: Color(0xFF12110D),
    panel: Color(0xFF14130F),
    panelHi: Color(0xFF1C1A14),
    glassTop: Color(0x13FFFFFF),
    glassBottom: Color(0x04FFFFFF),
    ink: Color(0xFFF4EAD2),
    mute: Color(0x99F4EAD2),
    faint: Color(0x40F4EAD2),
    gold: Color(0xFFD9B872),
    gold2: Color(0xFFF6E7BD),
    gold3: Color(0xFF8A6A2C),
    onGold: Color(0xFF1A1408),
    line: Color(0x33D9B872),
    lineStrong: Color(0x66D9B872),
    shadow: Color(0xFF000000),
    success: Color(0xFF7BE0A4),
    danger: Color(0xFFE8857A),
    live: Color(0xFF7BE0A4),
    sceneTop: Color(0xFF22201A),
    sceneBottom: Color(0xFF2B2315),
  );

  /// Day: warm ivory paper, bronze gold, ink text (same structure, AA contrast on every text role).
  static const LuxPalette day = LuxPalette(
    brightness: Brightness.light,
    bg: Color(0xFFF6F1E6),
    bg2: Color(0xFFEDE3CF),
    panel: Color(0xFFFFFCF5),
    panelHi: Color(0xFFFFFFFF),
    glassTop: Color(0xCCFFFFFF),
    glassBottom: Color(0x99FFFBF0),
    ink: Color(0xFF221C12),
    mute: Color(0xFF6B5E48),
    faint: Color(0x33221C12),
    gold: Color(0xFFA77B2C),
    gold2: Color(0xFFE5C98C),
    gold3: Color(0xFF6E4F17),
    onGold: Color(0xFF1A1408),
    line: Color(0x40A77B2C),
    lineStrong: Color(0x80A77B2C),
    shadow: Color(0xFF6E5A35),
    success: Color(0xFF2F8F5B),
    danger: Color(0xFFB5473B),
    live: Color(0xFF2F8F5B),
    sceneTop: Color(0xFFFFF8E8),
    sceneBottom: Color(0xFFE9DCC0),
  );

  static LuxPalette of(BuildContext context) => Theme.of(context).extension<LuxPalette>() ?? night;

  LinearGradient get goldGradient =>
      LinearGradient(begin: Alignment.topCenter, end: Alignment.bottomCenter, colors: <Color>[gold2, gold]);

  LinearGradient get glass =>
      LinearGradient(begin: const Alignment(-0.6, -1), end: const Alignment(0.6, 1), colors: <Color>[glassTop, glassBottom]);

  @override
  LuxPalette copyWith() => this;

  @override
  LuxPalette lerp(ThemeExtension<LuxPalette>? other, double t) {
    if (other is! LuxPalette) return this;
    Color l(Color a, Color b) => Color.lerp(a, b, t)!;
    return LuxPalette(
      brightness: t < .5 ? brightness : other.brightness,
      bg: l(bg, other.bg),
      bg2: l(bg2, other.bg2),
      panel: l(panel, other.panel),
      panelHi: l(panelHi, other.panelHi),
      glassTop: l(glassTop, other.glassTop),
      glassBottom: l(glassBottom, other.glassBottom),
      ink: l(ink, other.ink),
      mute: l(mute, other.mute),
      faint: l(faint, other.faint),
      gold: l(gold, other.gold),
      gold2: l(gold2, other.gold2),
      gold3: l(gold3, other.gold3),
      onGold: l(onGold, other.onGold),
      line: l(line, other.line),
      lineStrong: l(lineStrong, other.lineStrong),
      shadow: l(shadow, other.shadow),
      success: l(success, other.success),
      danger: l(danger, other.danger),
      live: l(live, other.live),
      sceneTop: l(sceneTop, other.sceneTop),
      sceneBottom: l(sceneBottom, other.sceneBottom),
    );
  }
}

/// Font families (bundled, SIL OFL 1.1).
const String kLuxSans = 'Vazirmatn';
const String kLuxSerif = 'CormorantGaramond';
const List<String> kLuxSansFallback = <String>['Estedad'];
const List<String> kLuxSerifFallback = <String>['Vazirmatn', 'Estedad'];

/// The one type scale. Persian / UI text: Vazirmatn; Latin display labels: Cormorant Garamond small caps.
class LuxType {
  LuxType._();

  static TextStyle _s(double size, FontWeight w, {double h = 1.45, double ls = 0}) => TextStyle(
      fontFamily: kLuxSans,
      fontFamilyFallback: kLuxSansFallback,
      fontSize: size,
      fontWeight: w,
      height: h,
      letterSpacing: ls);

  static final TextStyle display = _s(30, FontWeight.w300, h: 1.2);
  static final TextStyle title = _s(22, FontWeight.w400, h: 1.3);
  static final TextStyle headline = _s(18, FontWeight.w500, h: 1.35);
  static final TextStyle body = _s(14, FontWeight.w400);
  static final TextStyle bodyStrong = _s(14, FontWeight.w500);
  static final TextStyle label = _s(12.5, FontWeight.w500, h: 1.3);
  static final TextStyle caption = _s(11, FontWeight.w400, h: 1.35);
  static final TextStyle button = _s(14.5, FontWeight.w700, h: 1.2);

  /// Latin small-caps label (the reference's `.lat`).
  static const TextStyle latin = TextStyle(
      fontFamily: kLuxSerif,
      fontFamilyFallback: kLuxSerifFallback,
      fontSize: 12,
      fontWeight: FontWeight.w600,
      letterSpacing: 2.4,
      height: 1.2);

  /// Large Latin wordmark / numbers.
  static const TextStyle latinDisplay = TextStyle(
      fontFamily: kLuxSerif,
      fontFamilyFallback: kLuxSerifFallback,
      fontSize: 34,
      fontWeight: FontWeight.w600,
      letterSpacing: 5,
      height: 1.1);
}

class LuxSpace {
  LuxSpace._();
  static const double xs = 4, sm = 8, md = 14, lg = 20, xl = 28, page = 18;
}

class LuxRadius {
  LuxRadius._();
  static const double chip = 99, button = 18, panel = 26, card = 30, sheet = 32;
}

class LuxMotion {
  LuxMotion._();
  static const Curve ease = Cubic(.22, .8, .2, 1);
  static const Duration fast = Duration(milliseconds: 180);
  static const Duration normal = Duration(milliseconds: 380);
  static const Duration page = Duration(milliseconds: 560);
  static bool reduced(BuildContext context) => MediaQuery.maybeDisableAnimationsOf(context) ?? false;
}
