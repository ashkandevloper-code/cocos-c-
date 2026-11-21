import 'package:flutter/material.dart';

import '../theme/nx_theme.dart' show NxThemeColors;
import 'lux_tokens.dart';
import 'lux_transitions.dart';

/// Builds the whole app's Material theme in the Station Luxe language for one palette, so every Material widget
/// (buttons, fields, switches, tabs, dialogs, sheets, snack bars, list tiles, chips, sliders, progress) already looks
/// Luxe; screens compose the Lux components on top.
ThemeData buildLuxTheme(LuxPalette p) {
  final bool dark = p.isDark;
  final ColorScheme scheme = ColorScheme(
    brightness: p.brightness,
    primary: p.gold,
    onPrimary: p.onGold,
    primaryContainer: p.gold2,
    onPrimaryContainer: p.onGold,
    secondary: p.gold3,
    onSecondary: dark ? p.ink : Colors.white,
    tertiary: p.gold2,
    onTertiary: p.onGold,
    error: p.danger,
    onError: Colors.white,
    surface: p.panel,
    onSurface: p.ink,
    onSurfaceVariant: p.mute,
    surfaceContainerHighest: p.panelHi,
    surfaceContainerHigh: p.panelHi,
    surfaceContainer: p.panel,
    surfaceContainerLow: p.bg2,
    surfaceContainerLowest: p.bg,
    outline: p.lineStrong,
    outlineVariant: p.line,
    shadow: p.shadow,
    inverseSurface: p.ink,
    onInverseSurface: p.bg,
    inversePrimary: p.gold3,
    surfaceTint: Colors.transparent,
  );
  final TextTheme text = TextTheme(
    displayLarge: LuxType.display.copyWith(fontSize: 40),
    displayMedium: LuxType.display.copyWith(fontSize: 34),
    displaySmall: LuxType.display,
    headlineLarge: LuxType.title.copyWith(fontSize: 26),
    headlineMedium: LuxType.title,
    headlineSmall: LuxType.headline.copyWith(fontSize: 20),
    titleLarge: LuxType.headline,
    titleMedium: LuxType.bodyStrong.copyWith(fontSize: 15.5),
    titleSmall: LuxType.bodyStrong,
    bodyLarge: LuxType.body.copyWith(fontSize: 15),
    bodyMedium: LuxType.body,
    bodySmall: LuxType.caption.copyWith(fontSize: 12),
    labelLarge: LuxType.button,
    labelMedium: LuxType.label,
    labelSmall: LuxType.caption,
  ).apply(bodyColor: p.ink, displayColor: p.ink);

  final BorderRadius br = BorderRadius.circular(LuxRadius.button);
  final RoundedRectangleBorder buttonShape = RoundedRectangleBorder(borderRadius: br);

  // 3D gold: gradient face, light bevel on top, darker lip below, pressed = lip gone + face 2 px down.
  Widget goldFace(BuildContext context, Set<WidgetState> states, Widget? child) {
    final bool disabled = states.contains(WidgetState.disabled);
    final bool pressed = states.contains(WidgetState.pressed);
    return AnimatedContainer(
      duration: LuxMotion.fast,
      curve: LuxMotion.ease,
      transform: Matrix4.translationValues(0, pressed ? 2 : 0, 0),
      decoration: BoxDecoration(
        borderRadius: br,
        gradient: disabled
            ? LinearGradient(colors: <Color>[p.faint, p.faint])
            : LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: <Color>[p.gold2, p.gold, p.gold3.withValues(alpha: .95)],
                stops: const <double>[0, .55, 1]),
        border: Border.all(color: disabled ? p.line : p.gold2.withValues(alpha: .7), width: 1),
        boxShadow: disabled
            ? null
            : <BoxShadow>[
                BoxShadow(color: p.gold3, offset: Offset(0, pressed ? 1 : 4), blurRadius: 0),
                BoxShadow(
                    color: p.shadow.withValues(alpha: dark ? .55 : .25),
                    offset: Offset(0, pressed ? 4 : 12),
                    blurRadius: pressed ? 10 : 22,
                    spreadRadius: -6),
                if (dark) BoxShadow(color: p.gold.withValues(alpha: .28), blurRadius: 24, spreadRadius: -8),
              ],
      ),
      child: DecoratedBox(
        decoration: BoxDecoration(
          borderRadius: br,
          gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.center,
              colors: <Color>[Colors.white.withValues(alpha: disabled ? 0 : .35), Colors.white.withValues(alpha: 0)]),
        ),
        child: child,
      ),
    );
  }

  Widget glassFace(BuildContext context, Set<WidgetState> states, Widget? child) {
    final bool pressed = states.contains(WidgetState.pressed);
    return AnimatedContainer(
      duration: LuxMotion.fast,
      transform: Matrix4.translationValues(0, pressed ? 1.5 : 0, 0),
      decoration: BoxDecoration(
        borderRadius: br,
        gradient: p.glass,
        border: Border.all(color: states.contains(WidgetState.disabled) ? p.faint : p.lineStrong),
        boxShadow: <BoxShadow>[
          BoxShadow(color: p.shadow.withValues(alpha: dark ? .45 : .12), offset: Offset(0, pressed ? 2 : 8), blurRadius: 18, spreadRadius: -8),
        ],
      ),
      child: child,
    );
  }

  final ButtonStyle goldStyle = ButtonStyle(
    backgroundBuilder: goldFace,
    backgroundColor: const WidgetStatePropertyAll<Color>(Colors.transparent),
    foregroundColor: WidgetStateProperty.resolveWith((Set<WidgetState> s) => s.contains(WidgetState.disabled) ? p.mute : p.onGold),
    overlayColor: WidgetStatePropertyAll<Color>(Colors.white.withValues(alpha: .08)),
    elevation: const WidgetStatePropertyAll<double>(0),
    shadowColor: const WidgetStatePropertyAll<Color>(Colors.transparent),
    padding: const WidgetStatePropertyAll<EdgeInsets>(EdgeInsets.symmetric(horizontal: 22, vertical: 15)),
    minimumSize: const WidgetStatePropertyAll<Size>(Size(64, 50)),
    shape: WidgetStatePropertyAll<OutlinedBorder>(buttonShape),
    textStyle: WidgetStatePropertyAll<TextStyle>(LuxType.button),
  );
  final ButtonStyle glassStyle = ButtonStyle(
    backgroundBuilder: glassFace,
    backgroundColor: const WidgetStatePropertyAll<Color>(Colors.transparent),
    foregroundColor: WidgetStateProperty.resolveWith((Set<WidgetState> s) => s.contains(WidgetState.disabled) ? p.faint : p.ink),
    overlayColor: WidgetStatePropertyAll<Color>(p.gold.withValues(alpha: .08)),
    side: const WidgetStatePropertyAll<BorderSide>(BorderSide.none),
    padding: const WidgetStatePropertyAll<EdgeInsets>(EdgeInsets.symmetric(horizontal: 20, vertical: 14)),
    minimumSize: const WidgetStatePropertyAll<Size>(Size(64, 48)),
    shape: WidgetStatePropertyAll<OutlinedBorder>(buttonShape),
    textStyle: WidgetStatePropertyAll<TextStyle>(LuxType.bodyStrong),
  );

  final NxThemeColors legacy = NxThemeColors(
    accent: p.gold,
    accentSecondary: p.gold3,
    background: p.bg,
    backgroundMid: p.bg2,
    surface: p.panel.withValues(alpha: .92),
    surfaceRaised: p.panelHi,
    border: p.gold3,
    text: p.ink,
    muted: p.mute,
  );

  return ThemeData(
    useMaterial3: true,
    brightness: p.brightness,
    colorScheme: scheme,
    fontFamily: kLuxSans,
    fontFamilyFallback: kLuxSansFallback,
    textTheme: text,
    primaryTextTheme: text,
    scaffoldBackgroundColor: p.bg,
    canvasColor: p.bg,
    dividerColor: p.line,
    splashFactory: InkSparkle.splashFactory,
    extensions: <ThemeExtension<dynamic>>[p, legacy],
    pageTransitionsTheme: const PageTransitionsTheme(builders: <TargetPlatform, PageTransitionsBuilder>{
      TargetPlatform.android: LuxPageTransitionsBuilder(),
      TargetPlatform.iOS: LuxPageTransitionsBuilder(),
      TargetPlatform.linux: LuxPageTransitionsBuilder(),
      TargetPlatform.macOS: LuxPageTransitionsBuilder(),
      TargetPlatform.windows: LuxPageTransitionsBuilder(),
    }),
    appBarTheme: AppBarTheme(
      backgroundColor: Colors.transparent,
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      scrolledUnderElevation: 0,
      centerTitle: false,
      foregroundColor: p.ink,
      iconTheme: IconThemeData(color: dark ? p.gold2 : p.gold3, size: 22),
      titleTextStyle: LuxType.title.copyWith(color: p.ink, fontSize: 20),
    ),
    iconTheme: IconThemeData(color: p.ink, size: 22),
    cardTheme: CardThemeData(
      color: p.panel.withValues(alpha: dark ? .78 : .96),
      elevation: 0,
      margin: const EdgeInsets.symmetric(vertical: 6),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(LuxRadius.panel), side: BorderSide(color: p.line)),
    ),
    filledButtonTheme: FilledButtonThemeData(style: goldStyle),
    elevatedButtonTheme: ElevatedButtonThemeData(style: goldStyle),
    outlinedButtonTheme: OutlinedButtonThemeData(style: glassStyle),
    textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(
            foregroundColor: dark ? p.gold2 : p.gold3,
            textStyle: LuxType.bodyStrong,
            shape: buttonShape,
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10))),
    iconButtonTheme: IconButtonThemeData(style: IconButton.styleFrom(foregroundColor: dark ? p.gold2 : p.gold3)),
    floatingActionButtonTheme: FloatingActionButtonThemeData(
      backgroundColor: p.gold,
      foregroundColor: p.onGold,
      elevation: 6,
      shape: const StadiumBorder(),
      extendedTextStyle: LuxType.button,
    ),
    segmentedButtonTheme: SegmentedButtonThemeData(
      style: ButtonStyle(
        backgroundColor: WidgetStateProperty.resolveWith((Set<WidgetState> s) =>
            s.contains(WidgetState.selected) ? p.gold : p.panel.withValues(alpha: .6)),
        foregroundColor: WidgetStateProperty.resolveWith((Set<WidgetState> s) =>
            s.contains(WidgetState.selected) ? p.onGold : p.mute),
        side: WidgetStatePropertyAll<BorderSide>(BorderSide(color: p.line)),
        textStyle: WidgetStatePropertyAll<TextStyle>(LuxType.label),
        shape: const WidgetStatePropertyAll<OutlinedBorder>(StadiumBorder()),
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: dark ? const Color(0x0FFFFFFF) : Colors.white.withValues(alpha: .85),
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      border: OutlineInputBorder(borderRadius: br, borderSide: BorderSide(color: p.line)),
      enabledBorder: OutlineInputBorder(borderRadius: br, borderSide: BorderSide(color: p.line)),
      focusedBorder: OutlineInputBorder(borderRadius: br, borderSide: BorderSide(color: p.gold, width: 1.4)),
      errorBorder: OutlineInputBorder(borderRadius: br, borderSide: BorderSide(color: p.danger)),
      labelStyle: LuxType.body.copyWith(color: p.mute),
      floatingLabelStyle: LuxType.label.copyWith(color: dark ? p.gold2 : p.gold3),
      hintStyle: LuxType.body.copyWith(color: p.mute.withValues(alpha: .8)),
      prefixIconColor: p.mute,
      suffixIconColor: p.mute,
    ),
    listTileTheme: ListTileThemeData(
      iconColor: dark ? p.gold : p.gold3,
      textColor: p.ink,
      titleTextStyle: LuxType.bodyStrong.copyWith(color: p.ink, fontSize: 15),
      subtitleTextStyle: LuxType.caption.copyWith(color: p.mute, fontSize: 12),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
      contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 2),
    ),
    switchTheme: SwitchThemeData(
      thumbColor: WidgetStateProperty.resolveWith(
          (Set<WidgetState> s) => s.contains(WidgetState.selected) ? p.gold2 : (dark ? p.mute : Colors.white)),
      trackColor: WidgetStateProperty.resolveWith(
          (Set<WidgetState> s) => s.contains(WidgetState.selected) ? p.gold3 : p.faint),
      trackOutlineColor: WidgetStatePropertyAll<Color>(p.line),
    ),
    checkboxTheme: CheckboxThemeData(
      fillColor: WidgetStateProperty.resolveWith((Set<WidgetState> s) => s.contains(WidgetState.selected) ? p.gold : Colors.transparent),
      checkColor: WidgetStatePropertyAll<Color>(p.onGold),
      side: BorderSide(color: p.lineStrong, width: 1.4),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(6)),
    ),
    radioTheme: RadioThemeData(fillColor: WidgetStatePropertyAll<Color>(p.gold)),
    sliderTheme: SliderThemeData(
      activeTrackColor: p.gold,
      inactiveTrackColor: p.faint,
      thumbColor: p.gold2,
      overlayColor: p.gold.withValues(alpha: .15),
      valueIndicatorColor: p.panelHi,
      valueIndicatorTextStyle: LuxType.label.copyWith(color: p.ink),
      trackHeight: 3,
    ),
    progressIndicatorTheme: ProgressIndicatorThemeData(color: p.gold, linearTrackColor: p.faint, circularTrackColor: p.faint),
    tabBarTheme: TabBarThemeData(
      labelColor: p.onGold,
      unselectedLabelColor: p.mute,
      labelStyle: LuxType.label.copyWith(fontWeight: FontWeight.w700),
      unselectedLabelStyle: LuxType.label,
      dividerColor: Colors.transparent,
      indicatorSize: TabBarIndicatorSize.tab,
      tabAlignment: TabAlignment.start,
      indicator: BoxDecoration(
        borderRadius: BorderRadius.circular(99),
        gradient: p.goldGradient,
        boxShadow: <BoxShadow>[BoxShadow(color: p.gold.withValues(alpha: .45), blurRadius: 18, spreadRadius: -6)],
      ),
      overlayColor: const WidgetStatePropertyAll<Color>(Colors.transparent),
      splashFactory: NoSplash.splashFactory,
    ),
    chipTheme: ChipThemeData(
      backgroundColor: p.panel.withValues(alpha: .7),
      selectedColor: p.gold,
      disabledColor: p.faint,
      side: BorderSide(color: p.line),
      labelStyle: LuxType.label.copyWith(color: p.ink),
      secondaryLabelStyle: LuxType.label.copyWith(color: p.onGold),
      checkmarkColor: p.onGold,
      shape: const StadiumBorder(),
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
    ),
    dialogTheme: DialogThemeData(
      backgroundColor: p.panel,
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      titleTextStyle: LuxType.title.copyWith(color: p.ink, fontSize: 20),
      contentTextStyle: LuxType.body.copyWith(color: p.mute),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(LuxRadius.card), side: BorderSide(color: p.line)),
    ),
    bottomSheetTheme: BottomSheetThemeData(
      backgroundColor: p.panel,
      surfaceTintColor: Colors.transparent,
      modalBackgroundColor: p.panel,
      showDragHandle: true,
      dragHandleColor: p.faint,
      shape: RoundedRectangleBorder(
          borderRadius: const BorderRadius.vertical(top: Radius.circular(LuxRadius.sheet)), side: BorderSide(color: p.line)),
    ),
    snackBarTheme: SnackBarThemeData(
      behavior: SnackBarBehavior.floating,
      backgroundColor: dark ? const Color(0xF20E0E12) : p.ink,
      contentTextStyle: LuxType.label.copyWith(color: dark ? p.ink : p.bg),
      actionTextColor: p.gold2,
      elevation: 0,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(99), side: BorderSide(color: p.line)),
      insetPadding: const EdgeInsets.fromLTRB(24, 0, 24, 110),
    ),
    popupMenuTheme: PopupMenuThemeData(
      color: p.panel,
      surfaceTintColor: Colors.transparent,
      textStyle: LuxType.body.copyWith(color: p.ink),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20), side: BorderSide(color: p.line)),
    ),
    menuTheme: MenuThemeData(
      style: MenuStyle(
        backgroundColor: WidgetStatePropertyAll<Color>(p.panel),
        surfaceTintColor: const WidgetStatePropertyAll<Color>(Colors.transparent),
        shape: WidgetStatePropertyAll<OutlinedBorder>(
            RoundedRectangleBorder(borderRadius: BorderRadius.circular(20), side: BorderSide(color: p.line))),
      ),
    ),
    dropdownMenuTheme: DropdownMenuThemeData(textStyle: LuxType.body.copyWith(color: p.ink)),
    tooltipTheme: TooltipThemeData(
      decoration: BoxDecoration(color: dark ? const Color(0xF2141310) : p.ink, borderRadius: BorderRadius.circular(12), border: Border.all(color: p.line)),
      textStyle: LuxType.caption.copyWith(color: dark ? p.ink : p.bg, fontSize: 12),
      preferBelow: false,
    ),
    badgeTheme: BadgeThemeData(backgroundColor: p.gold, textColor: p.onGold),
    navigationBarTheme: NavigationBarThemeData(
      backgroundColor: p.panel.withValues(alpha: .9),
      indicatorColor: p.gold.withValues(alpha: .2),
      surfaceTintColor: Colors.transparent,
      labelTextStyle: WidgetStatePropertyAll<TextStyle>(LuxType.caption.copyWith(color: p.ink)),
      iconTheme: WidgetStatePropertyAll<IconThemeData>(IconThemeData(color: p.ink)),
    ),
    expansionTileTheme: ExpansionTileThemeData(
      iconColor: p.gold,
      collapsedIconColor: p.mute,
      textColor: p.ink,
      collapsedTextColor: p.ink,
      shape: const RoundedRectangleBorder(side: BorderSide.none),
      collapsedShape: const RoundedRectangleBorder(side: BorderSide.none),
    ),
    dividerTheme: DividerThemeData(color: p.line, thickness: 1, space: 1),
    textSelectionTheme: TextSelectionThemeData(cursorColor: p.gold, selectionColor: p.gold.withValues(alpha: .3), selectionHandleColor: p.gold),
  );
}
