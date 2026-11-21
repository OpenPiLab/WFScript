#pragma once

#include <QRect>
#include <QVector>
#include <QtGlobal>

// Design-time geometry helper only. Coordinates use Qt QWidget geometry's
// exclusive right/bottom boundary (x + width, y + height), not QRect::right().
// No Panel document/schema state is stored here.
namespace PanelSmartGuides {
struct Axis {
    bool matched = false;
    int delta = 0;
    int guide = 0;
};
struct Match {
    Axis x;
    Axis y;
};

inline int anchor(const QRect& r, int axis, int kind) {
    const int origin = axis == 0 ? r.x() : r.y();
    const int extent = axis == 0 ? r.width() : r.height();
    return origin + (kind == 0 ? 0 : kind == 1 ? extent / 2 : extent);
}

// Supported pairs: same edges, shared centre, and adjoining edges. We avoid
// aligning an edge to a centre because that tends to produce surprising drags.
inline Axis findAxis(const QRect& moving, const QVector<QRect>& stationary,
                     int axis, int tolerance = 6, bool resizeTrailingEdge = false) {
    Axis best;
    int bestDistance = tolerance + 1;
    int bestPriority = 99;
    const int movingKinds[] = {0, 2, 1}; // prioritize edges in equal-distance ties
    const int stationaryKinds[] = {0, 2, 1};
    for (int movingKind : movingKinds) {
        if (resizeTrailingEdge && movingKind != 2) continue;
        const int coordinate = anchor(moving, axis, movingKind);
        for (const QRect& peer : stationary) {
            for (int peerKind : stationaryKinds) {
                if ((movingKind == 1) != (peerKind == 1)) continue;
                const int line = anchor(peer, axis, peerKind);
                const int delta = line - coordinate;
                const int distance = qAbs(delta);
                const int priority = movingKind == peerKind ? 0 : 1;
                if (distance <= tolerance &&
                    (distance < bestDistance || (distance == bestDistance && priority < bestPriority))) {
                    best = {true, delta, line};
                    bestDistance = distance;
                    bestPriority = priority;
                }
            }
        }
    }
    return best;
}

inline Match match(const QRect& moving, const QVector<QRect>& stationary,
                   int tolerance = 6, bool trailingEdgeOnly = false) {
    return {findAxis(moving,stationary,0,tolerance,trailingEdgeOnly),
            findAxis(moving,stationary,1,tolerance,trailingEdgeOnly)};
}
}
