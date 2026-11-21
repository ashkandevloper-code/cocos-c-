import 'package:flame/game.dart';
import 'package:flutter/material.dart';

/// Flame owns the physical game's frame clock and canvas. Pure-Dart rules,
/// fixed-step physics, input validation and the ordered online move log remain
/// in the existing game cores: changing the renderer must not change outcomes.
class ArcadeFlameGame extends FlameGame {
  ArcadeFlameGame(this.onFrame);
  final void Function(double dt) onFrame;
  void Function(Canvas, Size)? draw;

  @override
  Color backgroundColor() => Colors.transparent;

  @override
  void update(double dt) {
    super.update(dt);
    onFrame(dt);
  }

  @override
  void render(Canvas canvas) {
    super.render(canvas);
    final viewport = canvasSize;
    if (viewport.x > 0 && viewport.y > 0) {
      draw?.call(canvas, Size(viewport.x, viewport.y));
    }
  }
}

/// An existing deterministic CustomPainter rendered by Flame. This is used
/// where a game has its own authoritative ticker (Finger Soccer, Billiards,
/// Carrom); Flame renders their boards without replacing their simulation.
class ArcadeFlameCanvas extends StatefulWidget {
  const ArcadeFlameCanvas({super.key, required this.painter});
  final CustomPainter painter;

  @override
  State<ArcadeFlameCanvas> createState() => _ArcadeFlameCanvasState();
}

class _ArcadeFlameCanvasState extends State<ArcadeFlameCanvas> {
  late final ArcadeFlameGame game = ArcadeFlameGame((_) {});

  @override
  Widget build(BuildContext context) {
    game.draw = widget.painter.paint;
    return ClipRect(child: GameWidget<ArcadeFlameGame>(game: game));
  }
}

/// GameWidget starts/stops Flame with the widget lifecycle. The previous
/// Flutter Ticker and CustomPaint frame loop are no longer used for these
/// arcade games. Other HUDs still listen to [frame] without rebuilding the
/// full canvas on every update.
abstract class ArcadeLoopState<T extends StatefulWidget> extends State<T> {
  late final ArcadeFlameGame _game = ArcadeFlameGame(_onFrame);
  final ValueNotifier<int> frame = ValueNotifier<int>(0);
  double time = 0;
  bool paused = false;

  void tick(double dt);

  Widget flameCanvas(void Function(Canvas canvas, Size size) draw) {
    _game.draw = draw;
    return ClipRect(child: GameWidget<ArcadeFlameGame>(game: _game));
  }

  void _onFrame(double dt) {
    if (!mounted || dt <= 0) return;
    // Preserve the old 1/20 s hitch clamp: a frame stall cannot tunnel a ball
    // through a rail or change the number of deterministic physics steps.
    final double safeDt = dt.clamp(0.0, 0.05).toDouble();
    if (!paused) {
      time += safeDt;
      tick(safeDt);
    }
    frame.value++;
  }

  @override
  void dispose() {
    _game.pauseEngine();
    frame.dispose();
    super.dispose();
  }
}

/// Still useful for small HUD canvases (e.g. kart minimap and pool power bar).
class ArcadePainter extends CustomPainter {
  ArcadePainter(this.onPaint, Listenable repaint) : super(repaint: repaint);
  final void Function(Canvas canvas, Size size) onPaint;

  @override
  void paint(Canvas canvas, Size size) => onPaint(canvas, size);

  @override
  bool shouldRepaint(covariant ArcadePainter oldDelegate) => true;
}
