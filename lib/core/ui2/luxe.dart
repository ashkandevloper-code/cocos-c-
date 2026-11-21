import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

/// ANIX PRO "Station Luxe" design tokens and primitives (black / gold, glass, 3D cards).
/// Native Flutter rebuild of the NEXORA Station Luxe HTML reference — no WebView.
class Luxe {
  Luxe._();

  static const Color bg = Color(0xFF07080C);
  static const Color ink = Color(0xFFF4EAD2);
  static const Color mute = Color(0x99F4EAD2);
  static const Color gold = Color(0xFFD9B872);
  static const Color gold2 = Color(0xFFF6E7BD);
  static const Color gold3 = Color(0xFF8A6A2C);
  static const Color line = Color(0x33D9B872);
  static const Color panel = Color(0xFF14130F);

  static const LinearGradient goldGrad =
      LinearGradient(begin: Alignment.topCenter, end: Alignment.bottomCenter, colors: <Color>[gold2, gold]);
  static const LinearGradient glass = LinearGradient(
      begin: Alignment(-0.6, -1),
      end: Alignment(0.6, 1),
      colors: <Color>[Color(0x13FFFFFF), Color(0x04FFFFFF)]);
  static const Curve ease = Cubic(.22, .8, .2, 1);

  static const String hubBg = 'assets/luxe/hub.webp';
  static const String splash = 'assets/luxe/splash.webp';
  static String icon(String n) => 'assets/luxe/icons/$n.webp';

  /// Latin small-caps label (the reference uses Cormorant Garamond; ChakraPetch is bundled).
  static const TextStyle latin =
      TextStyle(fontFamily: 'ChakraPetch', letterSpacing: 2.2, fontWeight: FontWeight.w600, color: gold, fontSize: 11);

  static bool reduceMotion(BuildContext context) => MediaQuery.maybeDisableAnimationsOf(context) ?? false;
}

/// Frosted glass surface with a hairline gold border.
class LuxeGlass extends StatelessWidget {
  const LuxeGlass({
    super.key,
    required this.child,
    this.radius = 26,
    this.padding = EdgeInsets.zero,
    this.blur = 14,
    this.color,
    this.glow,
  });
  final Widget child;
  final double radius;
  final EdgeInsetsGeometry padding;
  final double blur;
  final Color? color;
  final Color? glow;

  @override
  Widget build(BuildContext context) {
    final BorderRadius r = BorderRadius.circular(radius);
    return DecoratedBox(
      decoration: BoxDecoration(
        borderRadius: r,
        boxShadow: glow == null
            ? const <BoxShadow>[]
            : <BoxShadow>[BoxShadow(color: glow!.withValues(alpha: 0.35), blurRadius: 28, spreadRadius: -8)],
      ),
      child: ClipRRect(
        borderRadius: r,
        child: BackdropFilter(
          filter: ui.ImageFilter.blur(sigmaX: blur, sigmaY: blur),
          child: Container(
            padding: padding,
            decoration: BoxDecoration(
              borderRadius: r,
              color: color,
              gradient: color == null ? Luxe.glass : null,
              border: Border.all(color: Luxe.line),
            ),
            child: child,
          ),
        ),
      ),
    );
  }
}

/// Station Luxe's rounded surface without a backdrop filter. Use this for
/// scrolling lists (the home-style rim/shadow remains, but each row does not
/// create a costly offscreen blur layer).
class LuxeSurface extends StatelessWidget {
  const LuxeSurface({
    super.key,
    required this.child,
    this.radius = 26,
    this.padding = EdgeInsets.zero,
    this.color,
    this.borderColor = Luxe.line,
    this.glow = 0,
  });
  final Widget child;
  final double radius;
  final EdgeInsetsGeometry padding;
  final Color? color;
  final Color borderColor;
  final double glow;

  @override
  Widget build(BuildContext context) => Container(
        padding: padding,
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(radius),
          border: Border.all(color: borderColor),
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: <Color>[
              Color.lerp(color ?? Luxe.panel, Luxe.gold, 0.055)!,
              color ?? Luxe.panel,
            ],
          ),
          boxShadow: glow <= 0
              ? const <BoxShadow>[]
              : <BoxShadow>[
                  BoxShadow(
                    color: Luxe.gold.withValues(alpha: (glow * 0.24).clamp(0.0, 0.35)),
                    blurRadius: 28,
                    spreadRadius: -8,
                  ),
                ],
        ),
        child: child,
      );
}

/// Pressable with a soft scale, used by every Luxe card and button.
class LuxePress extends StatefulWidget {
  const LuxePress({super.key, required this.child, this.onTap, this.semantics});
  final Widget child;
  final VoidCallback? onTap;
  final String? semantics;

  @override
  State<LuxePress> createState() => _LuxePressState();
}

class _LuxePressState extends State<LuxePress> {
  bool _down = false;

  @override
  Widget build(BuildContext context) {
    return Semantics(
      button: true,
      label: widget.semantics,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTapDown: (_) => setState(() => _down = true),
        onTapCancel: () => setState(() => _down = false),
        onTapUp: (_) => setState(() => _down = false),
        onTap: widget.onTap,
        child: AnimatedScale(
          scale: _down ? 0.97 : 1,
          duration: const Duration(milliseconds: 160),
          curve: Luxe.ease,
          child: widget.child,
        ),
      ),
    );
  }
}

/// The ambient hub scene: warm radial light, the station art and the dark vignette.
class LuxeScene extends StatelessWidget {
  const LuxeScene({super.key, required this.child, this.artOpacity = 0.6, this.tint,
    this.animatedDust = true});
  final Widget child;
  final double artOpacity;
  final Color? tint;

  /// The home ring uses ambient motion; dense secondary pages keep the same
  /// lighting without an extra animation ticker or overdraw behind lists.
  final bool animatedDust;

  @override
  Widget build(BuildContext context) {
    return Stack(fit: StackFit.expand, children: <Widget>[
      const DecoratedBox(
        decoration: BoxDecoration(
          color: Luxe.bg,
          gradient: RadialGradient(
            center: Alignment(0, -1.15),
            radius: 1.2,
            colors: <Color>[Color(0xFF22201A), Luxe.bg],
          ),
        ),
      ),
      Opacity(
        opacity: artOpacity,
        child: Image.asset(Luxe.hubBg,
            fit: BoxFit.cover, cacheWidth: 900, errorBuilder: (_, __, ___) => const SizedBox.shrink()),
      ),
      DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            stops: const <double>[0, 0.4, 1],
            colors: <Color>[
              const Color(0x9907080C),
              (tint ?? const Color(0x2607080C)).withValues(alpha: tint == null ? 0.15 : 0.22),
              const Color(0xEB07080C),
            ],
          ),
        ),
      ),
      if (animatedDust) const RepaintBoundary(child: LuxeDust()),
      child,
    ]);
  }
}

/// Slow drifting gold dust. One ticker, ~40 particles, paused off-screen (TickerMode)
/// and disabled entirely when the system asks for reduced motion.
class LuxeDust extends StatefulWidget {
  const LuxeDust({super.key, this.count = 40});
  final int count;

  @override
  State<LuxeDust> createState() => _LuxeDustState();
}

class _LuxeDustState extends State<LuxeDust> with SingleTickerProviderStateMixin {
  late final AnimationController _c =
      AnimationController(vsync: this, duration: const Duration(seconds: 40));
  late final List<_Mote> _motes = List<_Mote>.generate(widget.count, (int i) => _Mote(math.Random(i * 7919 + 13)));

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (Luxe.reduceMotion(context)) {
      _c.stop();
    } else if (!_c.isAnimating) {
      _c.repeat();
    }
  }

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) =>
      IgnorePointer(child: CustomPaint(painter: _DustPainter(_c, _motes), size: Size.infinite));
}

class _Mote {
  _Mote(math.Random r)
      : x = r.nextDouble(),
        y = r.nextDouble(),
        size = 0.6 + r.nextDouble() * 1.6,
        speed = 0.3 + r.nextDouble() * 0.9,
        phase = r.nextDouble() * math.pi * 2,
        alpha = 0.15 + r.nextDouble() * 0.45;
  final double x, y, size, speed, phase, alpha;
}

class _DustPainter extends CustomPainter {
  _DustPainter(this.t, this.motes) : super(repaint: t);
  final Animation<double> t;
  final List<_Mote> motes;
  final Paint _p = Paint()..maskFilter = const MaskFilter.blur(BlurStyle.normal, 1.2);

  @override
  void paint(Canvas canvas, Size size) {
    final double v = t.value;
    for (final _Mote m in motes) {
      final double y = (m.y - v * m.speed) % 1.0;
      final double x = m.x + math.sin(v * math.pi * 2 * m.speed + m.phase) * 0.02;
      final double tw = 0.6 + 0.4 * math.sin(v * math.pi * 8 * m.speed + m.phase);
      _p.color = Luxe.gold2.withValues(alpha: m.alpha * tw);
      canvas.drawCircle(Offset(x * size.width, y * size.height), m.size, _p);
    }
  }

  @override
  bool shouldRepaint(_DustPainter old) => false;
}

/// Gold pill chip (wallet values, counters).
class LuxeChip extends StatelessWidget {
  const LuxeChip({super.key, required this.child, this.onTap, this.semantics});
  final Widget child;
  final VoidCallback? onTap;
  final String? semantics;

  @override
  Widget build(BuildContext context) => LuxePress(
        onTap: onTap,
        semantics: semantics,
        child: LuxeGlass(
          radius: 99,
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
          child: DefaultTextStyle.merge(
            style: const TextStyle(fontSize: 12, color: Luxe.ink, fontWeight: FontWeight.w600),
            child: child,
          ),
        ),
      );
}

/// Coin and gem glyphs drawn in gold (same shapes as the reference SVGs).
class LuxeCoin extends StatelessWidget {
  const LuxeCoin({super.key, this.size = 15});
  final double size;
  @override
  Widget build(BuildContext context) => CustomPaint(size: Size.square(size), painter: _CoinPainter());
}

class _CoinPainter extends CustomPainter {
  @override
  void paint(Canvas c, Size s) {
    final Offset o = s.center(Offset.zero);
    final double r = s.width / 2;
    c.drawCircle(o, r * .8, Paint()..color = Luxe.gold);
    c.drawCircle(o, r * .8, Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = r * .1
      ..color = Luxe.gold2);
    c.drawCircle(o, r * .48, Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = r * .1
      ..color = Luxe.gold3);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class LuxeGem extends StatelessWidget {
  const LuxeGem({super.key, this.size = 15});
  final double size;
  @override
  Widget build(BuildContext context) => CustomPaint(size: Size.square(size), painter: _GemPainter());
}

class _GemPainter extends CustomPainter {
  @override
  void paint(Canvas c, Size s) {
    final double w = s.width, h = s.height;
    final Path p = Path()
      ..moveTo(w * .5, h * .92)
      ..lineTo(w * .1, h * .38)
      ..lineTo(w * .27, h * .12)
      ..lineTo(w * .73, h * .12)
      ..lineTo(w * .9, h * .38)
      ..close();
    c.drawPath(p, Paint()..color = Luxe.gold2);
    c.drawPath(p, Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = w * .05
      ..color = Luxe.gold);
    c.drawLine(Offset(w * .1, h * .38), Offset(w * .9, h * .38), Paint()
      ..strokeWidth = w * .04
      ..color = Luxe.gold3);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

/// Thin XP / progress bar in gold.
class LuxeBar extends StatelessWidget {
  const LuxeBar({super.key, required this.value, this.height = 6});
  final double value;
  final double height;

  @override
  Widget build(BuildContext context) => ClipRRect(
        borderRadius: BorderRadius.circular(height),
        child: SizedBox(
          height: height,
          child: Stack(fit: StackFit.expand, children: <Widget>[
            const ColoredBox(color: Color(0x1FF6E7BD)),
            FractionallySizedBox(
              alignment: AlignmentDirectional.centerStart,
              widthFactor: value.clamp(0.0, 1.0),
              child: const DecoratedBox(
                decoration: BoxDecoration(
                    gradient: LinearGradient(colors: <Color>[Luxe.gold3, Luxe.gold, Luxe.gold2])),
              ),
            ),
          ]),
        ),
      );
}

/// Bottom sheet chrome used across ANIX PRO (grab handle, dark gold panel).
Future<T?> showLuxeSheet<T>(BuildContext context, {required WidgetBuilder builder}) {
  return showModalBottomSheet<T>(
    context: context,
    useSafeArea: true,
    isScrollControlled: true,
    backgroundColor: Colors.transparent,
    barrierColor: const Color(0x8C030306),
    builder: (BuildContext ctx) => Container(
      padding: EdgeInsets.fromLTRB(20, 12, 20, MediaQuery.paddingOf(ctx).bottom + 22),
      decoration: const BoxDecoration(
        borderRadius: BorderRadius.vertical(top: Radius.circular(32)),
        gradient: LinearGradient(
            begin: Alignment.topCenter, end: Alignment.bottomCenter, colors: <Color>[Color(0xFF14130F), Color(0xFF0A0A0D)]),
        border: Border(top: BorderSide(color: Luxe.line)),
      ),
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Container(
            width: 40,
            height: 4,
            margin: const EdgeInsets.only(bottom: 14),
            decoration: BoxDecoration(color: const Color(0x40F6E7BD), borderRadius: BorderRadius.circular(4))),
        Flexible(child: builder(ctx)),
      ]),
    ),
  );
}

/// One wide glass option row in a Luxe sheet.
class LuxeOption extends StatelessWidget {
  const LuxeOption({super.key, required this.label, this.icon, this.onTap, this.trailing});
  final String label;
  final String? icon;
  final VoidCallback? onTap;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.only(top: 9),
        child: LuxePress(
          onTap: onTap,
          semantics: label,
          child: LuxeGlass(
            radius: 20,
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
            child: Row(children: <Widget>[
              if (icon != null)
                Image.asset(Luxe.icon(icon!), width: 32, height: 32, cacheWidth: 96,
                    errorBuilder: (_, __, ___) => const SizedBox(width: 32)),
              if (icon != null) const SizedBox(width: 12),
              Expanded(child: Text(label, style: const TextStyle(fontSize: 14, color: Luxe.ink))),
              if (trailing != null) trailing!,
            ]),
          ),
        ),
      );
}
