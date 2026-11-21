import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'lux_tokens.dart';

/// Station Luxe page transition: the new page rises out of depth (slight forward tilt, scale 0.92 -> 1, soft blur-like
/// fade) while the page underneath recedes and dims, like walking from the hub into a station. Calm, 3D, never a
/// slide. Reduced motion: a plain cross-fade.
class LuxDepthTransition extends StatelessWidget {
  const LuxDepthTransition({super.key, required this.animation, required this.secondary, required this.child});

  final Animation<double> animation;
  final Animation<double> secondary;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    if (LuxMotion.reduced(context)) {
      return FadeTransition(opacity: animation, child: child);
    }
    final Animation<double> inA = CurvedAnimation(parent: animation, curve: LuxMotion.ease, reverseCurve: Curves.easeInCubic);
    final Animation<double> outA = CurvedAnimation(parent: secondary, curve: LuxMotion.ease);
    return AnimatedBuilder(
      animation: Listenable.merge(<Listenable>[inA, outA]),
      child: child,
      builder: (BuildContext context, Widget? c) {
        final double t = inA.value; // 0 -> 1 as this page arrives
        final double o = outA.value; // 0 -> 1 as a page arrives on top of this one
        final double scale = (0.92 + 0.08 * t) * (1 - 0.06 * o);
        final double tilt = (1 - t) * 0.16 - o * 0.05; // radians around X: rises from below the eye line
        final Matrix4 m = Matrix4.identity()
          ..setEntry(3, 2, 0.0012)
          ..translateByDouble(0.0, (1 - t) * 26 - o * 10, 0.0, 1.0)
          ..rotateX(tilt)
          ..scaleByDouble(scale, scale, 1.0, 1.0);
        final double opacity = (t.clamp(0.0, 1.0) * (1 - 0.35 * o)).toDouble();
        return Opacity(
          opacity: math.max(0.0, math.min(1.0, opacity)),
          child: Transform(alignment: Alignment.center, transform: m, child: c),
        );
      },
    );
  }
}

class LuxPageTransitionsBuilder extends PageTransitionsBuilder {
  const LuxPageTransitionsBuilder();

  @override
  Duration get transitionDuration => LuxMotion.page;

  @override
  Widget buildTransitions<T>(PageRoute<T> route, BuildContext context, Animation<double> animation,
          Animation<double> secondaryAnimation, Widget child) =>
      LuxDepthTransition(animation: animation, secondary: secondaryAnimation, child: child);
}
