#include "CursorPassElement.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <cmath>

#include <hyprland/src/render/Renderer.hpp>

using namespace Hyprutils::Utils;

CCursorPassElement::CCursorPassElement(const CCursorPassElement::SRenderData& data) : m_data(data) {
    ;
}

std::vector<UP<IPassElement>> CCursorPassElement::draw() {
    Mat3x3 transform = toTransform(m_data.box, m_data.box.rot, m_data.hotspot, m_data.stretchAngle, m_data.stretchMagnitude);
    m_data.box.rot   = 0;

    drawCursor(transform, m_data.tex, m_data.box, g_pHyprRenderer->m_renderData.damage, m_data.nearest, m_data.opacity);

    return {}; // no passes to be submitted later
}

bool CCursorPassElement::needsLiveBlur() {
    return false; // TODO?
}

bool CCursorPassElement::needsPrecomputeBlur() {
    return false; // TODO?
}

std::optional<CBox> CCursorPassElement::boundingBox() {
    // Rotation is around the hotspot, which may be far from the center of
    // a magnified shape. Include every rotated corner in pass culling.
    const auto pivot = m_data.box.pos() + m_data.hotspot;
    const double radius = std::hypot(m_data.box.w, m_data.box.h) *
                          std::max({1.0, m_data.stretchMagnitude.x, m_data.stretchMagnitude.y});
    return CBox{pivot - Vector2D{radius, radius}, Vector2D{2 * radius, 2 * radius}}
        .scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();
}

CRegion CCursorPassElement::opaqueRegion() {
    return {}; // TODO:
}

void CCursorPassElement::discard() {
    ;
}
