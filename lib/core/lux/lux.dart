// ANIX PRO "Station Luxe" component library. Every screen is composed from these (plus the Luxe Material theme in
// lux_theme.dart), so the whole app speaks one language in both night and day modes.
import 'dart:async';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'lux_tokens.dart';

export 'lux_theme.dart';
export 'lux_tokens.dart';
export 'lux_transitions.dart';

// ============================================================================ scene
/// The ambient Station Luxe backdrop: warm radial light from above and below, the hub artwork faded in (night), and
/// slow drifting gold dust. Every page sits on it.
class LuxScene extends StatelessWidget {
  const LuxScene({super.key, this.child, this.art = 'assets/luxe/hub.webp', this.artOpacity, this.accent, this.dust = true});

  final Widget? child;
  final String? art;
  final double? artOpacity;

  /// Optional world colour washed in from the top (a station's own hue).
  final Color? accent;
  final bool dust;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Stack(fit: StackFit.expand, children: <Widget>[
      DecoratedBox(
        decoration: BoxDecoration(
          color: p.bg,
          gradient: RadialGradient(
              center: const Alignment(0, -1.15), radius: 1.2, colors: <Color>[p.sceneTop, p.bg], stops: const <double>[0, .62]),
        ),
      ),
      DecoratedBox(
        decoration: BoxDecoration(
          gradient: RadialGradient(
              center: const Alignment(0, 1.25),
              radius: 1.0,
              colors: <Color>[p.sceneBottom, p.sceneBottom.withValues(alpha: 0)],
              stops: const <double>[0, .62]),
        ),
      ),
      if (art != null)
        Opacity(
          opacity: artOpacity ?? (p.isDark ? .30 : .10),
          child: ExcludeSemantics(
            child: Image.asset(art!,
                fit: BoxFit.cover,
                errorBuilder: (_, __, ___) => const SizedBox.shrink(),
                color: p.isDark ? null : const Color(0xFFF6F1E6),
                colorBlendMode: p.isDark ? null : BlendMode.luminosity),
          ),
        ),
      if (accent != null)
        DecoratedBox(
          decoration: BoxDecoration(
            gradient: RadialGradient(
                center: const Alignment(0, -1),
                radius: .9,
                colors: <Color>[accent!.withValues(alpha: p.isDark ? .22 : .12), accent!.withValues(alpha: 0)]),
          ),
        ),
      DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: <Color>[p.bg.withValues(alpha: .55), p.bg.withValues(alpha: .10), p.bg.withValues(alpha: .9)],
              stops: const <double>[0, .4, 1]),
        ),
      ),
      if (dust && !LuxMotion.reduced(context)) const IgnorePointer(child: LuxDust()),
      if (child != null) child!,
    ]);
  }
}

class LuxDust extends StatefulWidget {
  const LuxDust({super.key, this.count = 34});
  final int count;
  @override
  State<LuxDust> createState() => _LuxDustState();
}

class _LuxDustState extends State<LuxDust> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: const Duration(seconds: 40))..repeat();
  late final List<List<double>> _m = List<List<double>>.generate(widget.count, (int i) {
    final math.Random r = math.Random(i * 7919 + 13);
    return <double>[r.nextDouble(), r.nextDouble(), .4 + r.nextDouble() * 1.4, .2 + r.nextDouble() * .8, r.nextDouble()];
  });

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => RepaintBoundary(
        child: CustomPaint(painter: _DustPainter(_c, _m, LuxPalette.of(context).gold2), size: Size.infinite),
      );
}

class _DustPainter extends CustomPainter {
  _DustPainter(this.a, this.m, this.color) : super(repaint: a);
  final Animation<double> a;
  final List<List<double>> m;
  final Color color;
  @override
  void paint(Canvas canvas, Size size) {
    final double t = a.value;
    for (final List<double> d in m) {
      final double y = (d[1] - t * d[3]) % 1.0;
      final double x = d[0] + math.sin((t * 6.28 * d[3]) + d[4] * 6.28) * .02;
      final double tw = .35 + .65 * (0.5 + 0.5 * math.sin(t * 40 * d[3] + d[4] * 9));
      canvas.drawCircle(Offset(x * size.width, y * size.height), d[2],
          Paint()..color = color.withValues(alpha: .10 + .30 * tw));
    }
  }

  @override
  bool shouldRepaint(_DustPainter old) => false;
}

// ============================================================================ surfaces
/// Frosted glass panel with a hairline gold border (the reference's `.ic` / `.prof` surfaces).
class LuxPanel extends StatelessWidget {
  const LuxPanel({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(16),
    this.radius = LuxRadius.panel,
    this.glow,
    this.onTap,
    this.blur = true,
    this.margin,
    this.highlight = false,
  });

  final Widget child;
  final EdgeInsetsGeometry padding;
  final EdgeInsetsGeometry? margin;
  final double radius;
  final Color? glow;
  final VoidCallback? onTap;
  final bool blur;

  /// A gold-tinted, slightly raised panel (the selected / featured one).
  final bool highlight;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final BorderRadius r = BorderRadius.circular(radius);
    Widget body = DecoratedBox(
      decoration: BoxDecoration(
        borderRadius: r,
        gradient: highlight
            ? LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: <Color>[p.gold.withValues(alpha: p.isDark ? .16 : .14), p.glassBottom])
            : p.glass,
        color: p.isDark ? null : p.panel,
        border: Border.all(color: highlight ? p.lineStrong : p.line),
        boxShadow: <BoxShadow>[
          BoxShadow(color: p.shadow.withValues(alpha: p.isDark ? .5 : .10), blurRadius: 30, offset: const Offset(0, 16), spreadRadius: -14),
          if (glow != null) BoxShadow(color: glow!.withValues(alpha: .35), blurRadius: 40, spreadRadius: -16),
        ],
      ),
      child: Padding(padding: padding, child: child),
    );
    if (blur && p.isDark) {
      body = ClipRRect(borderRadius: r, child: BackdropFilter(filter: ui.ImageFilter.blur(sigmaX: 14, sigmaY: 14), child: body));
    }
    if (onTap != null) body = LuxPress(onTap: onTap, child: body);
    return margin == null ? body : Padding(padding: margin!, child: body);
  }
}

/// Press feedback: a slight 3D push (scale + tilt toward the finger) and a light haptic.
class LuxPress extends StatefulWidget {
  const LuxPress({super.key, required this.child, this.onTap, this.onLongPress, this.scale = .97, this.haptic = true});
  final Widget child;
  final VoidCallback? onTap;
  final VoidCallback? onLongPress;
  final double scale;
  final bool haptic;
  @override
  State<LuxPress> createState() => _LuxPressState();
}

class _LuxPressState extends State<LuxPress> {
  bool _down = false;
  @override
  Widget build(BuildContext context) {
    final bool enabled = widget.onTap != null || widget.onLongPress != null;
    return Semantics(
      button: enabled,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTapDown: enabled ? (_) => setState(() => _down = true) : null,
        onTapCancel: () => setState(() => _down = false),
        onTapUp: (_) => setState(() => _down = false),
        onTap: widget.onTap == null
            ? null
            : () {
                if (widget.haptic) HapticFeedback.selectionClick();
                widget.onTap!();
              },
        onLongPress: widget.onLongPress,
        child: AnimatedScale(
          scale: _down ? widget.scale : 1,
          duration: LuxMotion.fast,
          curve: LuxMotion.ease,
          child: widget.child,
        ),
      ),
    );
  }
}

// ============================================================================ buttons
enum LuxButtonKind { gold, glass, danger, ghost }

/// The 3D gold button: champagne gradient face, a bright bevel, a bronze lip underneath that the face sinks into on
/// press. [LuxButtonKind.glass] is the secondary, [danger] destructive, [ghost] text-only.
class LuxButton extends StatefulWidget {
  const LuxButton({
    super.key,
    required this.label,
    this.onPressed,
    this.icon,
    this.kind = LuxButtonKind.gold,
    this.expand = false,
    this.busy = false,
    this.compact = false,
  });

  const LuxButton.glass({
    super.key,
    required this.label,
    this.onPressed,
    this.icon,
    this.expand = false,
    this.busy = false,
    this.compact = false,
  }) : kind = LuxButtonKind.glass;

  final String label;
  final VoidCallback? onPressed;
  final IconData? icon;
  final LuxButtonKind kind;
  final bool expand;
  final bool busy;
  final bool compact;

  @override
  State<LuxButton> createState() => _LuxButtonState();
}

class _LuxButtonState extends State<LuxButton> {
  bool _down = false;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final bool enabled = widget.onPressed != null && !widget.busy;
    final double h = widget.compact ? 40 : 52;
    final double lip = widget.kind == LuxButtonKind.gold || widget.kind == LuxButtonKind.danger ? 4 : 0;
    final BorderRadius br = BorderRadius.circular(widget.compact ? 14 : LuxRadius.button);
    late final Gradient face;
    late final Color fg, lipColor, border;
    switch (widget.kind) {
      case LuxButtonKind.gold:
        face = LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: <Color>[p.gold2, p.gold, Color.lerp(p.gold, p.gold3, .55)!],
            stops: const <double>[0, .55, 1]);
        fg = p.onGold;
        lipColor = p.gold3;
        border = p.gold2.withValues(alpha: .75);
      case LuxButtonKind.danger:
        face = LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: <Color>[Color.lerp(p.danger, Colors.white, .25)!, p.danger]);
        fg = Colors.white;
        lipColor = Color.lerp(p.danger, Colors.black, .45)!;
        border = Color.lerp(p.danger, Colors.white, .4)!;
      case LuxButtonKind.glass:
        face = p.glass;
        fg = p.ink;
        lipColor = Colors.transparent;
        border = p.lineStrong;
      case LuxButtonKind.ghost:
        face = const LinearGradient(colors: <Color>[Colors.transparent, Colors.transparent]);
        fg = p.isDark ? p.gold2 : p.gold3;
        lipColor = Colors.transparent;
        border = Colors.transparent;
    }
    final double sink = _down ? lip - 1 : 0;
    Widget content = Row(
      mainAxisSize: widget.expand ? MainAxisSize.max : MainAxisSize.min,
      mainAxisAlignment: MainAxisAlignment.center,
      children: <Widget>[
        if (widget.busy)
          SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2, color: fg))
        else if (widget.icon != null)
          Icon(widget.icon, size: widget.compact ? 18 : 20, color: fg),
        if ((widget.icon != null || widget.busy) && widget.label.isNotEmpty) const SizedBox(width: 8),
        if (widget.label.isNotEmpty)
          Flexible(
            child: Text(widget.label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: (widget.compact ? LuxType.label.copyWith(fontWeight: FontWeight.w700) : LuxType.button).copyWith(color: fg)),
          ),
      ],
    );
    final Widget button = AnimatedContainer(
      duration: LuxMotion.fast,
      curve: LuxMotion.ease,
      height: h,
      margin: EdgeInsets.only(top: sink, bottom: lip - sink),
      padding: EdgeInsets.symmetric(horizontal: widget.compact ? 14 : 22),
      decoration: BoxDecoration(
        borderRadius: br,
        gradient: face,
        color: widget.kind == LuxButtonKind.glass && !p.isDark ? p.panel : null,
        border: Border.all(color: border),
        boxShadow: <BoxShadow>[
          if (lip > 0) BoxShadow(color: lipColor, offset: Offset(0, lip - sink), blurRadius: 0),
          if (widget.kind != LuxButtonKind.ghost)
            BoxShadow(
                color: p.shadow.withValues(alpha: p.isDark ? .55 : .18),
                offset: Offset(0, _down ? 4 : 12),
                blurRadius: _down ? 8 : 22,
                spreadRadius: -8),
          if (widget.kind == LuxButtonKind.gold && p.isDark && enabled)
            BoxShadow(color: p.gold.withValues(alpha: .30), blurRadius: 26, spreadRadius: -10),
        ],
      ),
      foregroundDecoration: widget.kind == LuxButtonKind.gold || widget.kind == LuxButtonKind.danger
          ? BoxDecoration(
              borderRadius: br,
              gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.center,
                  colors: <Color>[Colors.white.withValues(alpha: .30), Colors.white.withValues(alpha: 0)]),
            )
          : null,
      child: content,
    );
    return Semantics(
      button: true,
      enabled: enabled,
      label: widget.label,
      child: Opacity(
        opacity: enabled || widget.busy ? 1 : .45,
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTapDown: enabled ? (_) => setState(() => _down = true) : null,
          onTapCancel: () => setState(() => _down = false),
          onTapUp: (_) => setState(() => _down = false),
          onTap: enabled
              ? () {
                  HapticFeedback.lightImpact();
                  widget.onPressed!();
                }
              : null,
          child: ExcludeSemantics(child: button),
        ),
      ),
    );
  }
}

/// Round glass icon button (the reference's back button / header chips).
class LuxIconButton extends StatelessWidget {
  const LuxIconButton({super.key, required this.icon, this.onPressed, this.tooltip, this.size = 42, this.badge, this.gold = false});
  final IconData icon;
  final VoidCallback? onPressed;
  final String? tooltip;
  final double size;
  final int? badge;
  final bool gold;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    Widget w = LuxPress(
      onTap: onPressed,
      child: Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: gold ? p.goldGradient : p.glass,
          color: gold || p.isDark ? null : p.panel,
          border: Border.all(color: gold ? p.gold2 : p.line),
          boxShadow: <BoxShadow>[
            BoxShadow(color: p.shadow.withValues(alpha: p.isDark ? .4 : .1), blurRadius: 14, offset: const Offset(0, 6), spreadRadius: -6),
          ],
        ),
        child: Icon(icon, size: size * .45, color: gold ? p.onGold : (p.isDark ? p.gold2 : p.gold3)),
      ),
    );
    if (badge != null && badge! > 0) {
      w = Badge(label: Text(badge! > 99 ? '99+' : '$badge'), child: w);
    }
    if (tooltip != null) w = Tooltip(message: tooltip!, child: w);
    return Semantics(label: tooltip, button: true, child: w);
  }
}

// ============================================================================ header / page shell
/// Page header: round back button, optional emblem orb, title + Latin kicker, trailing actions.
class LuxHeader extends StatelessWidget {
  const LuxHeader({
    super.key,
    required this.title,
    this.kicker,
    this.icon,
    this.iconAsset,
    this.accent,
    this.actions = const <Widget>[],
    this.onBack,
    this.showBack = true,
  });

  final String title;

  /// Small Latin caps line above the title (e.g. "ARCADE").
  final String? kicker;
  final IconData? icon;
  final String? iconAsset;
  final Color? accent;
  final List<Widget> actions;
  final VoidCallback? onBack;
  final bool showBack;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Color c = accent ?? p.gold;
    final bool canPop = Navigator.of(context).canPop();
    return Padding(
      padding: const EdgeInsets.fromLTRB(LuxSpace.page, 6, LuxSpace.page, 10),
      child: Row(children: <Widget>[
        if (icon != null || iconAsset != null) ...<Widget>[
          Container(
            width: 50,
            height: 50,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: RadialGradient(center: const Alignment(-.3, -.4), colors: <Color>[c.withValues(alpha: .34), c.withValues(alpha: 0)]),
              border: Border.all(color: c.withValues(alpha: .45)),
              boxShadow: <BoxShadow>[BoxShadow(color: c.withValues(alpha: .35), blurRadius: 26, spreadRadius: -10)],
            ),
            alignment: Alignment.center,
            child: iconAsset != null
                ? Image.asset(iconAsset!, width: 40, height: 40, errorBuilder: (_, __, ___) => Icon(icon ?? Icons.star_rounded, color: c))
                : Icon(icon, color: p.isDark ? c : p.gold3, size: 23),
          ),
          const SizedBox(width: 12),
        ],
        Expanded(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, mainAxisSize: MainAxisSize.min, children: <Widget>[
            if (kicker != null) Text(kicker!.toUpperCase(), style: LuxType.latin.copyWith(color: p.isDark ? p.gold : p.gold3, fontSize: 11)),
            Text(title,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: LuxType.title.copyWith(color: p.ink, fontSize: 21)),
          ]),
        ),
        ...actions.expand((Widget a) => <Widget>[const SizedBox(width: 8), a]),
        if (showBack && (canPop || onBack != null)) ...<Widget>[
          const SizedBox(width: 10),
          LuxIconButton(
            key: const ValueKey<String>('lux-back'),
            icon: Directionality.of(context) == TextDirection.rtl ? Icons.arrow_forward_rounded : Icons.arrow_back_rounded,
            tooltip: MaterialLocalizations.of(context).backButtonTooltip,
            onPressed: onBack ?? () => Navigator.of(context).maybePop(),
          ),
        ],
      ]),
    );
  }
}

/// The page shell every screen uses: Luxe scene, safe area, [LuxHeader], body, optional bottom bar / FAB.
class LuxPage extends StatelessWidget {
  const LuxPage({
    super.key,
    required this.title,
    required this.body,
    this.kicker,
    this.icon,
    this.iconAsset,
    this.accent,
    this.actions = const <Widget>[],
    this.onBack,
    this.showBack = true,
    this.bottom,
    this.floating,
    this.tabs,
    this.scroll = false,
    this.padding = const EdgeInsets.fromLTRB(LuxSpace.page, 0, LuxSpace.page, 24),
    this.art = 'assets/luxe/hub.webp',
    this.resizeToAvoidBottomInset = true,
  });

  final String title;
  final String? kicker;
  final IconData? icon;
  final String? iconAsset;
  final Color? accent;
  final List<Widget> actions;
  final VoidCallback? onBack;
  final bool showBack;
  final Widget body;
  final Widget? bottom;
  final Widget? floating;

  /// Pill tabs under the header (pass a [LuxPillTabs]).
  final Widget? tabs;

  /// Wrap [body] in a padded scroll view.
  final bool scroll;
  final EdgeInsetsGeometry padding;
  final String? art;
  final bool resizeToAvoidBottomInset;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    Widget content = body;
    if (scroll) {
      content = ListView(padding: padding, physics: const BouncingScrollPhysics(), children: <Widget>[body]);
    }
    return AnnotatedRegion<SystemUiOverlayStyle>(
      value: p.isDark ? SystemUiOverlayStyle.light : SystemUiOverlayStyle.dark,
      child: Scaffold(
        backgroundColor: p.bg,
        resizeToAvoidBottomInset: resizeToAvoidBottomInset,
        floatingActionButton: floating,
        bottomNavigationBar: bottom,
        body: LuxScene(
          art: art,
          accent: accent,
          child: SafeArea(
            bottom: bottom == null,
            child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
              LuxHeader(
                  title: title,
                  kicker: kicker,
                  icon: icon,
                  iconAsset: iconAsset,
                  accent: accent,
                  actions: actions,
                  onBack: onBack,
                  showBack: showBack),
              if (tabs != null) Padding(padding: const EdgeInsets.only(bottom: 8), child: tabs),
              Expanded(child: content),
            ]),
          ),
        ),
      ),
    );
  }
}

// ============================================================================ tabs / toggles
/// Horizontal pill tabs (the reference's `#wtabs`). Selected = gold gradient pill with a glow.
class LuxPillTabs extends StatelessWidget {
  const LuxPillTabs({super.key, required this.labels, required this.index, required this.onChanged, this.icons, this.padding});
  final List<String> labels;
  final List<IconData>? icons;
  final int index;
  final ValueChanged<int> onChanged;
  final EdgeInsetsGeometry? padding;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return SizedBox(
      height: 42,
      child: ListView.separated(
        scrollDirection: Axis.horizontal,
        padding: padding ?? const EdgeInsets.symmetric(horizontal: LuxSpace.page),
        itemCount: labels.length,
        separatorBuilder: (_, __) => const SizedBox(width: 8),
        itemBuilder: (BuildContext context, int i) {
          final bool on = i == index;
          return Semantics(
            selected: on,
            button: true,
            child: LuxPress(
              onTap: () => onChanged(i),
              child: AnimatedContainer(
                duration: LuxMotion.normal,
                curve: LuxMotion.ease,
                padding: const EdgeInsets.symmetric(horizontal: 16),
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(99),
                  gradient: on ? p.goldGradient : null,
                  color: on ? null : (p.isDark ? Colors.transparent : p.panel.withValues(alpha: .7)),
                  border: Border.all(color: on ? Colors.transparent : p.line),
                  boxShadow: on ? <BoxShadow>[BoxShadow(color: p.gold.withValues(alpha: .5), blurRadius: 22, spreadRadius: -6)] : null,
                ),
                child: Row(mainAxisSize: MainAxisSize.min, children: <Widget>[
                  if (icons != null && i < icons!.length) ...<Widget>[
                    Icon(icons![i], size: 16, color: on ? p.onGold : p.mute),
                    const SizedBox(width: 6),
                  ],
                  Text(labels[i], style: LuxType.label.copyWith(color: on ? p.onGold : p.mute, fontWeight: on ? FontWeight.w700 : FontWeight.w500)),
                ]),
              ),
            ),
          );
        },
      ),
    );
  }
}

/// Gold toggle switch with a sliding jewel.
class LuxToggle extends StatelessWidget {
  const LuxToggle({super.key, required this.value, required this.onChanged, this.semanticLabel});
  final bool value;
  final ValueChanged<bool>? onChanged;
  final String? semanticLabel;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Semantics(
      toggled: value,
      label: semanticLabel,
      child: GestureDetector(
        onTap: onChanged == null
            ? null
            : () {
                HapticFeedback.selectionClick();
                onChanged!(!value);
              },
        child: AnimatedContainer(
          duration: LuxMotion.normal,
          curve: LuxMotion.ease,
          width: 52,
          height: 30,
          padding: const EdgeInsets.all(3),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(99),
            gradient: value ? LinearGradient(colors: <Color>[p.gold3, p.gold]) : null,
            color: value ? null : p.faint,
            border: Border.all(color: value ? p.gold2.withValues(alpha: .6) : p.line),
            boxShadow: value ? <BoxShadow>[BoxShadow(color: p.gold.withValues(alpha: .35), blurRadius: 14, spreadRadius: -4)] : null,
          ),
          child: AnimatedAlign(
            duration: LuxMotion.normal,
            curve: LuxMotion.ease,
            alignment: value ? AlignmentDirectional.centerEnd : AlignmentDirectional.centerStart,
            child: Container(
              width: 24,
              height: 24,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                gradient: RadialGradient(center: const Alignment(-.4, -.5), colors: <Color>[Colors.white, value ? p.gold2 : const Color(0xFFD9D4C8)]),
                boxShadow: const <BoxShadow>[BoxShadow(color: Color(0x66000000), blurRadius: 4, offset: Offset(0, 2))],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

/// Gold checkbox (rounded square, champagne fill, dark tick).
class LuxCheckbox extends StatelessWidget {
  const LuxCheckbox({super.key, required this.value, required this.onChanged, this.semanticLabel});
  final bool value;
  final ValueChanged<bool>? onChanged;
  final String? semanticLabel;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Semantics(
      checked: value,
      label: semanticLabel,
      child: GestureDetector(
        onTap: onChanged == null ? null : () => onChanged!(!value),
        child: AnimatedContainer(
          duration: LuxMotion.fast,
          width: 24,
          height: 24,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(7),
            gradient: value ? p.goldGradient : null,
            border: Border.all(color: value ? p.gold2 : p.lineStrong, width: 1.4),
          ),
          child: value ? Icon(Icons.check_rounded, size: 18, color: p.onGold) : null,
        ),
      ),
    );
  }
}

// ============================================================================ rows / sections
/// List row: leading orb (icon / image), title + subtitle, trailing (value, toggle, chevron).
class LuxListRow extends StatelessWidget {
  const LuxListRow({
    super.key,
    required this.title,
    this.subtitle,
    this.icon,
    this.leading,
    this.trailing,
    this.onTap,
    this.accent,
    this.chevron,
    this.danger = false,
    this.dense = false,
  });

  final String title;
  final String? subtitle;
  final IconData? icon;
  final Widget? leading;
  final Widget? trailing;
  final VoidCallback? onTap;
  final Color? accent;

  /// Show a chevron (defaults to true when [onTap] is set and there is no [trailing]).
  final bool? chevron;
  final bool danger;
  final bool dense;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Color c = danger ? p.danger : (accent ?? (p.isDark ? p.gold : p.gold3));
    final bool showChevron = chevron ?? (onTap != null && trailing == null);
    final Widget row = Padding(
      padding: EdgeInsets.symmetric(horizontal: 4, vertical: dense ? 8 : 11),
      child: Row(children: <Widget>[
        if (leading != null)
          leading!
        else if (icon != null)
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: RadialGradient(center: const Alignment(-.3, -.4), colors: <Color>[c.withValues(alpha: .28), c.withValues(alpha: .02)]),
              border: Border.all(color: c.withValues(alpha: .35)),
            ),
            child: Icon(icon, size: 19, color: c),
          ),
        if (leading != null || icon != null) const SizedBox(width: 12),
        Expanded(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, mainAxisSize: MainAxisSize.min, children: <Widget>[
            Text(title, style: LuxType.bodyStrong.copyWith(color: danger ? p.danger : p.ink, fontSize: 14.5)),
            if (subtitle != null && subtitle!.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(top: 2),
                child: Text(subtitle!, style: LuxType.caption.copyWith(color: p.mute, fontSize: 11.5)),
              ),
          ]),
        ),
        if (trailing != null) ...<Widget>[const SizedBox(width: 10), trailing!],
        if (showChevron)
          Icon(Directionality.of(context) == TextDirection.rtl ? Icons.chevron_left_rounded : Icons.chevron_right_rounded,
              color: p.mute, size: 22),
      ]),
    );
    if (onTap == null) return row;
    return LuxPress(onTap: onTap, scale: .985, child: row);
  }
}

/// A titled group of rows inside one panel, with hairline separators.
class LuxSection extends StatelessWidget {
  const LuxSection({super.key, this.title, required this.children, this.trailing, this.padding});
  final String? title;
  final Widget? trailing;
  final List<Widget> children;
  final EdgeInsetsGeometry? padding;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 14),
      child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
        if (title != null) LuxSectionTitle(title!, trailing: trailing),
        LuxPanel(
          padding: padding ?? const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
          child: Column(children: <Widget>[
            for (int i = 0; i < children.length; i++) ...<Widget>[
              if (i > 0) Divider(height: 1, color: p.line),
              children[i],
            ],
          ]),
        ),
      ]),
    );
  }
}

class LuxSectionTitle extends StatelessWidget {
  const LuxSectionTitle(this.text, {super.key, this.trailing, this.latin});
  final String text;
  final String? latin;
  final Widget? trailing;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Padding(
      padding: const EdgeInsets.fromLTRB(6, 10, 6, 8),
      child: Row(children: <Widget>[
        Container(width: 18, height: 1, color: p.gold.withValues(alpha: .7)),
        const SizedBox(width: 8),
        Expanded(
          child: Text.rich(TextSpan(children: <InlineSpan>[
            TextSpan(text: text, style: LuxType.label.copyWith(color: p.isDark ? p.gold2 : p.gold3, fontSize: 13)),
            if (latin != null) TextSpan(text: '  ${latin!.toUpperCase()}', style: LuxType.latin.copyWith(color: p.mute, fontSize: 10)),
          ])),
        ),
        if (trailing != null) trailing!,
      ]),
    );
  }
}

/// Small glass chip (currency, counts, live dots).
class LuxChip extends StatelessWidget {
  const LuxChip({super.key, required this.label, this.icon, this.leading, this.onTap, this.selected = false, this.live = false});
  final String label;
  final IconData? icon;
  final Widget? leading;
  final VoidCallback? onTap;
  final bool selected;
  final bool live;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Widget chip = AnimatedContainer(
      duration: LuxMotion.fast,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(99),
        gradient: selected ? p.goldGradient : p.glass,
        color: selected || p.isDark ? null : p.panel,
        border: Border.all(color: selected ? Colors.transparent : p.line),
      ),
      child: Row(mainAxisSize: MainAxisSize.min, children: <Widget>[
        if (live) ...<Widget>[const LuxLiveDot(), const SizedBox(width: 6)],
        if (leading != null) ...<Widget>[leading!, const SizedBox(width: 6)],
        if (icon != null) ...<Widget>[Icon(icon, size: 15, color: selected ? p.onGold : (p.isDark ? p.gold : p.gold3)), const SizedBox(width: 5)],
        Text(label, style: LuxType.label.copyWith(color: selected ? p.onGold : p.ink, fontSize: 12)),
      ]),
    );
    return onTap == null ? chip : LuxPress(onTap: onTap, child: chip);
  }
}

class LuxLiveDot extends StatefulWidget {
  const LuxLiveDot({super.key, this.color});
  final Color? color;
  @override
  State<LuxLiveDot> createState() => _LuxLiveDotState();
}

class _LuxLiveDotState extends State<LuxLiveDot> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: const Duration(milliseconds: 1800))..repeat(reverse: true);
  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final Color c = widget.color ?? LuxPalette.of(context).live;
    return FadeTransition(
      opacity: Tween<double>(begin: 1, end: .35).animate(_c),
      child: Container(
        width: 7,
        height: 7,
        decoration: BoxDecoration(shape: BoxShape.circle, color: c, boxShadow: <BoxShadow>[BoxShadow(color: c, blurRadius: 8)]),
      ),
    );
  }
}

/// Thin gold progress bar (XP etc.).
class LuxProgress extends StatelessWidget {
  const LuxProgress({super.key, required this.value, this.height = 4});
  final double value;
  final double height;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return ClipRRect(
      borderRadius: BorderRadius.circular(height),
      child: Container(
        height: height,
        color: p.faint,
        alignment: AlignmentDirectional.centerStart,
        child: FractionallySizedBox(
          widthFactor: value.clamp(0.0, 1.0).toDouble(),
          child: DecoratedBox(decoration: BoxDecoration(gradient: LinearGradient(colors: <Color>[p.gold3, p.gold2]))),
        ),
      ),
    );
  }
}

// ============================================================================ states
class LuxEmpty extends StatelessWidget {
  const LuxEmpty({super.key, required this.title, this.message, this.icon = Icons.auto_awesome_rounded, this.action, this.actionLabel});
  final String title;
  final String? message;
  final IconData icon;
  final VoidCallback? action;
  final String? actionLabel;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(28),
        child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
          Container(
            width: 84,
            height: 84,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: RadialGradient(colors: <Color>[p.gold.withValues(alpha: .22), p.gold.withValues(alpha: 0)]),
              border: Border.all(color: p.line),
            ),
            child: Icon(icon, size: 34, color: p.isDark ? p.gold : p.gold3),
          ),
          const SizedBox(height: 16),
          Text(title, textAlign: TextAlign.center, style: LuxType.headline.copyWith(color: p.ink)),
          if (message != null) ...<Widget>[
            const SizedBox(height: 6),
            Text(message!, textAlign: TextAlign.center, style: LuxType.body.copyWith(color: p.mute)),
          ],
          if (action != null && actionLabel != null) ...<Widget>[
            const SizedBox(height: 18),
            LuxButton(label: actionLabel!, onPressed: action),
          ],
        ]),
      ),
    );
  }
}

/// Loading state: a slowly turning gold ring with a hairline sweep.
class LuxLoading extends StatefulWidget {
  const LuxLoading({super.key, this.label, this.size = 46});
  final String? label;
  final double size;
  @override
  State<LuxLoading> createState() => _LuxLoadingState();
}

class _LuxLoadingState extends State<LuxLoading> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: const Duration(milliseconds: 1400))..repeat();
  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Center(
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        SizedBox(
          width: widget.size,
          height: widget.size,
          child: RotationTransition(
            turns: _c,
            child: CustomPaint(painter: _RingPainter(p.gold, p.faint)),
          ),
        ),
        if (widget.label != null) ...<Widget>[
          const SizedBox(height: 12),
          Text(widget.label!, style: LuxType.label.copyWith(color: p.mute)),
        ],
      ]),
    );
  }
}

class _RingPainter extends CustomPainter {
  _RingPainter(this.c, this.track);
  final Color c, track;
  @override
  void paint(Canvas canvas, Size s) {
    final Rect r = Offset.zero & s;
    canvas.drawArc(r.deflate(2), 0, math.pi * 2, false, Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..color = track);
    canvas.drawArc(
        r.deflate(2),
        0,
        math.pi * .7,
        false,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2.6
          ..strokeCap = StrokeCap.round
          ..shader = SweepGradient(colors: <Color>[c.withValues(alpha: 0), c]).createShader(r));
  }

  @override
  bool shouldRepaint(_RingPainter o) => o.c != c;
}

// ============================================================================ dialogs / sheets / toast
/// Luxe dialog: glass card, gold title, actions as Lux buttons. Returns what the action pops.
Future<T?> showLuxDialog<T>(
  BuildContext context, {
  required String title,
  String? message,
  Widget? content,
  IconData? icon,
  List<LuxDialogAction<T>> actions = const <Never>[],
  bool barrierDismissible = true,
}) {
  return showGeneralDialog<T>(
    context: context,
    barrierDismissible: barrierDismissible,
    barrierLabel: MaterialLocalizations.of(context).modalBarrierDismissLabel,
    barrierColor: const Color(0x99030306),
    transitionDuration: LuxMotion.normal,
    pageBuilder: (BuildContext ctx, _, __) {
      final LuxPalette p = LuxPalette.of(ctx);
      return SafeArea(
        child: Center(
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 26),
            child: Material(
              type: MaterialType.transparency,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 420),
                child: Container(
                  padding: const EdgeInsets.fromLTRB(22, 24, 22, 18),
                  decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(LuxRadius.card),
                    gradient: LinearGradient(
                        begin: Alignment.topCenter,
                        end: Alignment.bottomCenter,
                        colors: p.isDark ? <Color>[const Color(0xFF1A1812), const Color(0xFF0C0C10)] : <Color>[p.panelHi, p.panel]),
                    border: Border.all(color: p.lineStrong),
                    boxShadow: <BoxShadow>[BoxShadow(color: p.shadow.withValues(alpha: p.isDark ? .7 : .2), blurRadius: 60, offset: const Offset(0, 26), spreadRadius: -18)],
                  ),
                  child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
                    if (icon != null) ...<Widget>[
                      Icon(icon, size: 34, color: p.isDark ? p.gold : p.gold3),
                      const SizedBox(height: 10),
                    ],
                    Text(title, textAlign: TextAlign.center, style: LuxType.title.copyWith(color: p.ink, fontSize: 20)),
                    if (message != null) ...<Widget>[
                      const SizedBox(height: 8),
                      Text(message, textAlign: TextAlign.center, style: LuxType.body.copyWith(color: p.mute)),
                    ],
                    if (content != null) ...<Widget>[const SizedBox(height: 14), content],
                    if (actions.isNotEmpty) const SizedBox(height: 18),
                    for (int i = 0; i < actions.length; i++) ...<Widget>[
                      if (i > 0) const SizedBox(height: 8),
                      LuxButton(
                        label: actions[i].label,
                        kind: actions[i].kind,
                        expand: true,
                        onPressed: () => Navigator.of(ctx).pop(actions[i].value),
                      ),
                    ],
                  ]),
                ),
              ),
            ),
          ),
        ),
      );
    },
    transitionBuilder: (BuildContext ctx, Animation<double> a, Animation<double> s, Widget child) {
      final Animation<double> c = CurvedAnimation(parent: a, curve: LuxMotion.ease);
      return FadeTransition(opacity: c, child: ScaleTransition(scale: Tween<double>(begin: .94, end: 1).animate(c), child: child));
    },
  );
}

class LuxDialogAction<T> {
  const LuxDialogAction(this.label, this.value, {this.kind = LuxButtonKind.gold});
  final String label;
  final T value;
  final LuxButtonKind kind;
}

/// Yes / no confirm in one call.
Future<bool> luxConfirm(BuildContext context,
    {required String title, String? message, required String yes, required String no, bool danger = false}) async {
  final bool? r = await showLuxDialog<bool>(context, title: title, message: message, actions: <LuxDialogAction<bool>>[
    LuxDialogAction<bool>(yes, true, kind: danger ? LuxButtonKind.danger : LuxButtonKind.gold),
    LuxDialogAction<bool>(no, false, kind: LuxButtonKind.glass),
  ]);
  return r ?? false;
}

/// Luxe bottom sheet (rounded glass, grab handle, title / subtitle).
Future<T?> showLuxSheet<T>(BuildContext context, {String? title, String? subtitle, required WidgetBuilder builder, bool scroll = true}) {
  return showModalBottomSheet<T>(
    context: context,
    isScrollControlled: true,
    useSafeArea: true,
    backgroundColor: Colors.transparent,
    barrierColor: const Color(0x8C030306),
    showDragHandle: false,
    builder: (BuildContext ctx) {
      final LuxPalette p = LuxPalette.of(ctx);
      final Widget inner = Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
        Center(
          child: Container(width: 40, height: 4, margin: const EdgeInsets.only(bottom: 14), decoration: BoxDecoration(color: p.faint, borderRadius: BorderRadius.circular(4))),
        ),
        if (title != null) Text(title, textAlign: TextAlign.center, style: LuxType.title.copyWith(color: p.ink, fontSize: 20)),
        if (subtitle != null)
          Padding(
            padding: const EdgeInsets.only(top: 4, bottom: 6),
            child: Text(subtitle, textAlign: TextAlign.center, style: LuxType.label.copyWith(color: p.mute)),
          ),
        if (title != null) const SizedBox(height: 10),
        Flexible(child: scroll ? SingleChildScrollView(child: builder(ctx)) : builder(ctx)),
      ]);
      return Padding(
        padding: EdgeInsets.only(bottom: MediaQuery.viewInsetsOf(ctx).bottom),
        child: Container(
          padding: EdgeInsets.fromLTRB(20, 12, 20, 22 + MediaQuery.paddingOf(ctx).bottom),
          decoration: BoxDecoration(
            borderRadius: const BorderRadius.vertical(top: Radius.circular(LuxRadius.sheet)),
            gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: p.isDark ? <Color>[const Color(0xFF14130F), const Color(0xFF0A0A0D)] : <Color>[p.panelHi, p.panel]),
            border: Border(top: BorderSide(color: p.lineStrong)),
          ),
          child: inner,
        ),
      );
    },
  );
}

/// One option row in a sheet (the reference's `.opt`).
class LuxOption extends StatelessWidget {
  const LuxOption({super.key, required this.label, this.icon, this.asset, this.onTap, this.subtitle});
  final String label;
  final String? subtitle;
  final IconData? icon;
  final String? asset;
  final VoidCallback? onTap;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Padding(
      padding: const EdgeInsets.only(top: 9),
      child: LuxPress(
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(20),
            gradient: p.glass,
            color: p.isDark ? null : p.panelHi,
            border: Border.all(color: p.line),
          ),
          child: Row(children: <Widget>[
            if (asset != null) Image.asset(asset!, width: 30, height: 30, errorBuilder: (_, __, ___) => const SizedBox(width: 30)),
            if (icon != null) Icon(icon, color: p.isDark ? p.gold : p.gold3),
            if (asset != null || icon != null) const SizedBox(width: 12),
            Expanded(
              child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: <Widget>[
                Text(label, style: LuxType.bodyStrong.copyWith(color: p.ink)),
                if (subtitle != null) Text(subtitle!, style: LuxType.caption.copyWith(color: p.mute)),
              ]),
            ),
          ]),
        ),
      ),
    );
  }
}

/// Floating pill toast above the dock (the reference's `#toast`). Replaces the previous one.
class LuxToast {
  LuxToast._();
  static OverlayEntry? _entry;
  static Timer? _timer;

  static void show(BuildContext context, String message, {IconData? icon, Duration duration = const Duration(milliseconds: 2400)}) {
    final OverlayState? overlay = Overlay.maybeOf(context, rootOverlay: true);
    if (overlay == null) return;
    _timer?.cancel();
    _entry?.remove();
    final LuxPalette p = LuxPalette.of(context);
    final OverlayEntry e = OverlayEntry(
      builder: (BuildContext ctx) => Positioned(
        left: 24,
        right: 24,
        bottom: 116 + MediaQuery.paddingOf(ctx).bottom,
        child: IgnorePointer(
          child: Center(
            child: TweenAnimationBuilder<double>(
              tween: Tween<double>(begin: 0, end: 1),
              duration: LuxMotion.normal,
              curve: LuxMotion.ease,
              builder: (_, double t, Widget? c) => Opacity(opacity: t, child: Transform.translate(offset: Offset(0, 10 * (1 - t)), child: c)),
              child: Material(
                type: MaterialType.transparency,
                child: Container(
                  key: const ValueKey<String>('lux-toast'),
                  padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 11),
                  decoration: BoxDecoration(
                    color: p.isDark ? const Color(0xE60E0E12) : p.ink,
                    borderRadius: BorderRadius.circular(99),
                    border: Border.all(color: p.line),
                  ),
                  child: Row(mainAxisSize: MainAxisSize.min, children: <Widget>[
                    if (icon != null) ...<Widget>[Icon(icon, size: 16, color: p.gold2), const SizedBox(width: 8)],
                    Flexible(child: Text(message, style: LuxType.label.copyWith(color: p.isDark ? p.ink : p.bg))),
                  ]),
                ),
              ),
            ),
          ),
        ),
      ),
    );
    _entry = e;
    overlay.insert(e);
    _timer = Timer(duration, () {
      if (_entry == e) {
        e.remove();
        _entry = null;
      }
    });
  }
}

// ============================================================================ text field
class LuxTextField extends StatelessWidget {
  const LuxTextField({
    super.key,
    this.controller,
    this.label,
    this.hint,
    this.icon,
    this.obscure = false,
    this.keyboardType,
    this.onSubmitted,
    this.onChanged,
    this.maxLength,
    this.textAlign = TextAlign.start,
    this.autofocus = false,
    this.suffix,
    this.maxLines = 1,
    this.inputFormatters,
    this.textDirection,
  });
  final TextEditingController? controller;
  final String? label;
  final String? hint;
  final IconData? icon;
  final bool obscure;
  final TextInputType? keyboardType;
  final ValueChanged<String>? onSubmitted;
  final ValueChanged<String>? onChanged;
  final int? maxLength;
  final TextAlign textAlign;
  final bool autofocus;
  final Widget? suffix;
  final int maxLines;
  final List<TextInputFormatter>? inputFormatters;
  final TextDirection? textDirection;
  @override
  Widget build(BuildContext context) => TextField(
        controller: controller,
        obscureText: obscure,
        keyboardType: keyboardType,
        onSubmitted: onSubmitted,
        onChanged: onChanged,
        maxLength: maxLength,
        textAlign: textAlign,
        autofocus: autofocus,
        maxLines: maxLines,
        inputFormatters: inputFormatters,
        textDirection: textDirection,
        style: LuxType.body.copyWith(color: LuxPalette.of(context).ink, fontSize: 15),
        decoration: InputDecoration(
          labelText: label,
          hintText: hint,
          prefixIcon: icon == null ? null : Icon(icon),
          suffixIcon: suffix,
          counterText: '',
        ),
      );
}

// ============================================================================ tiles
/// A category / feature tile (the reference's 2-column `.ic` cards): glowing orb or art, title, caption, gentle 3D
/// tilt on press, staggered rise-in.
class LuxTile extends StatelessWidget {
  const LuxTile({
    super.key,
    required this.title,
    this.subtitle,
    this.icon,
    this.asset,
    this.accent,
    this.onTap,
    this.index = 0,
    this.badge,
  });
  final String title;
  final String? subtitle;
  final IconData? icon;
  final String? asset;
  final Color? accent;
  final VoidCallback? onTap;
  final int index;
  final String? badge;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Color c = accent ?? p.gold;
    final Widget card = LuxPanel(
      onTap: onTap,
      padding: const EdgeInsets.fromLTRB(12, 18, 12, 16),
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Stack(clipBehavior: Clip.none, children: <Widget>[
          if (asset != null)
            SizedBox(
              height: 70,
              child: Image.asset(asset!, fit: BoxFit.contain, errorBuilder: (_, __, ___) => Icon(icon ?? Icons.auto_awesome, color: c, size: 32)),
            )
          else
            Container(
              width: 62,
              height: 62,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                gradient: RadialGradient(center: const Alignment(-.3, -.4), colors: <Color>[c.withValues(alpha: .34), c.withValues(alpha: .0)]),
                border: Border.all(color: c.withValues(alpha: .4)),
                boxShadow: <BoxShadow>[BoxShadow(color: c.withValues(alpha: .4), blurRadius: 28, spreadRadius: -10)],
              ),
              child: Icon(icon ?? Icons.auto_awesome, color: p.isDark ? c : Color.lerp(c, Colors.black, .25), size: 27),
            ),
          if (badge != null)
            PositionedDirectional(
              top: -4,
              end: -10,
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 2),
                decoration: BoxDecoration(gradient: p.goldGradient, borderRadius: BorderRadius.circular(99)),
                child: Text(badge!, style: LuxType.caption.copyWith(color: p.onGold, fontWeight: FontWeight.w700, fontSize: 10)),
              ),
            ),
        ]),
        const SizedBox(height: 12),
        Text(title, textAlign: TextAlign.center, maxLines: 1, overflow: TextOverflow.ellipsis, style: LuxType.bodyStrong.copyWith(color: p.ink, fontSize: 14.5)),
        if (subtitle != null) ...<Widget>[
          const SizedBox(height: 3),
          Text(subtitle!,
              textAlign: TextAlign.center,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: LuxType.caption.copyWith(color: p.mute, fontSize: 10.5)),
        ],
      ]),
    );
    if (LuxMotion.reduced(context)) return card;
    return TweenAnimationBuilder<double>(
      tween: Tween<double>(begin: 0, end: 1),
      duration: Duration(milliseconds: 520 + 70 * index.clamp(0, 10)),
      curve: LuxMotion.ease,
      builder: (_, double t, Widget? c) => Opacity(opacity: t, child: Transform.translate(offset: Offset(0, 16 * (1 - t)), child: c)),
      child: card,
    );
  }
}

/// Two-column grid of [LuxTile]s that sizes to its content.
class LuxTileGrid extends StatelessWidget {
  const LuxTileGrid({super.key, required this.children, this.columns = 2, this.spacing = 14});
  final List<Widget> children;
  final int columns;
  final double spacing;
  @override
  Widget build(BuildContext context) {
    final List<Widget> rows = <Widget>[];
    for (int i = 0; i < children.length; i += columns) {
      rows.add(Padding(
        padding: EdgeInsets.only(bottom: spacing),
        child: IntrinsicHeight(
          child: Row(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
            for (int j = 0; j < columns; j++) ...<Widget>[
              if (j > 0) SizedBox(width: spacing),
              Expanded(child: i + j < children.length ? children[i + j] : const SizedBox.shrink()),
            ],
          ]),
        ),
      ));
    }
    return Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: rows);
  }
}

// ============================================================================ avatar / medallion
/// Round avatar in a champagne-gold ring (initials or an image), optional level badge.
class LuxAvatar extends StatelessWidget {
  const LuxAvatar({super.key, this.size = 42, this.initials = '', this.image, this.level});
  final double size;
  final String initials;
  final ImageProvider? image;
  final int? level;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Widget ring = Container(
      width: size,
      height: size,
      padding: const EdgeInsets.all(1.6),
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        gradient: SweepGradient(colors: <Color>[p.gold2, p.gold3, p.gold2, p.gold3, p.gold2]),
        boxShadow: <BoxShadow>[BoxShadow(color: p.gold.withValues(alpha: .25), blurRadius: 14)],
      ),
      child: ClipOval(
        child: Container(
          color: p.isDark ? const Color(0xFF0C0D12) : p.panelHi,
          alignment: Alignment.center,
          child: image != null
              ? Image(image: image!, fit: BoxFit.cover, width: size, height: size, errorBuilder: (_, __, ___) => const SizedBox.shrink())
              : Text(initials.isEmpty ? '?' : initials.characters.take(2).toString().toUpperCase(),
                  style: LuxType.latin.copyWith(fontSize: size * .36, color: p.ink, letterSpacing: 1)),
        ),
      ),
    );
    if (level == null) return ring;
    return Stack(clipBehavior: Clip.none, children: <Widget>[
      ring,
      PositionedDirectional(
        bottom: -4,
        end: -4,
        child: Container(
          constraints: const BoxConstraints(minWidth: 20),
          height: 20,
          padding: const EdgeInsets.symmetric(horizontal: 5),
          alignment: Alignment.center,
          decoration: BoxDecoration(gradient: p.goldGradient, borderRadius: BorderRadius.circular(10), border: Border.all(color: p.bg, width: 1.5)),
          child: Text('$level', style: LuxType.latin.copyWith(fontSize: 11, color: p.onGold, letterSpacing: 0)),
        ),
      ),
    ]);
  }
}

/// The owner's luxe icon art (dark-backed 3D medallions) inside a round jewel frame that reads in night and day.
class LuxMedallion extends StatelessWidget {
  const LuxMedallion({super.key, required this.asset, this.size = 64, this.glow});
  final String asset;
  final double size;
  final Color? glow;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Color g = glow ?? p.gold;
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        color: const Color(0xFF0A0A0E),
        border: Border.all(color: g.withValues(alpha: .55), width: 1.2),
        boxShadow: <BoxShadow>[
          BoxShadow(color: g.withValues(alpha: p.isDark ? .35 : .25), blurRadius: size * .4, spreadRadius: -size * .12),
          BoxShadow(color: p.shadow.withValues(alpha: p.isDark ? .5 : .18), blurRadius: 12, offset: const Offset(0, 6)),
        ],
      ),
      child: ClipOval(
        child: Image.asset(asset,
            fit: BoxFit.cover,
            cacheWidth: (size * 2.6).round(),
            errorBuilder: (_, __, ___) => Icon(Icons.auto_awesome_rounded, color: g, size: size * .45)),
      ),
    );
  }
}
