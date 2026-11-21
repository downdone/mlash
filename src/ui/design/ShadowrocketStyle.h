#pragma once

#include "UiDesignTokens.h"

namespace mlash::design::ShadowrocketStyle {

inline constexpr Geometry geometry{30,28,8,8, 14,12,12,200, 42,32,34,36, 36,38,52,350, 10,2,16,76,4};
inline constexpr int narrowSidebarWidth=144;
inline constexpr int narrowSidebarBreakpoint=960;
inline constexpr int collapsedSidebarWidth=44;
inline constexpr int navigationRowHeight=38;
inline constexpr Typography typography{10.5,10.5,9, 13,11.5,15,13};
inline constexpr int brandPoints=16;
inline constexpr const char* artworkSurface="#f2f3f6";
inline constexpr const char* scheduleFill(bool dark) { return dark ? "#173b29" : "#def3e6"; }

// Daymark DesignTokens colors, mapped to the existing Mlash widget roles.
inline constexpr Colors light{
    "#ffffff","#f3f4f6","#f6f6f8",
    "#1c1c1e","#5d5d66","#5d5d66","#989ca5",
    "#e0e1e7","#e0e1e7","#0063cf","#eaf1fc",
    "#0063cf","#ffffff","#f0f2f6","#0063cf",
    "#f2f3f6","#edf0f5","#005bbb",
    "#236e46","#945000","#b92330","#787883",artworkSurface,"#248a3d"};
inline constexpr Colors dark{
    "#1c1c1e","#171719","#252527",
    "#f2f2f7","#b7b7c2","#b7b7c2","#6f7580",
    "#424249","#424249","#83baff","#263a52",
    "#83baff","#102038","#303034","#83baff",
    "#2b2b30","#38383e","#90c5ff",
    "#83d6aa","#ffc18c","#ff9ca4","#8b8b97",artworkSurface,"#32d45a"};
inline constexpr Colors highContrastLight{
    "#ffffff","#f3f4f6","#f6f6f8",
    "#1c1c1e","#484a50","#484a50","#989ca5",
    "#626772","#9fa5b0","#004ca8","#d9e8ff",
    "#004ca8","#ffffff","#f0f2f6","#004ca8",
    "#f2f3f6","#edf0f5","#004ca8",
    "#236e46","#945000","#b92330","#626772",artworkSurface,"#248a3d"};
inline constexpr Colors highContrastDark{
    "#1c1c1e","#171719","#252527",
    "#f2f2f7","#ced2da","#ced2da","#6f7580",
    "#a4afbe","#77808d","#a7ceff","#29466d",
    "#a7ceff","#102038","#303034","#a7ceff",
    "#2b2b30","#38383e","#a7ceff",
    "#83d6aa","#ffc18c","#ff9ca4","#a4afbe",artworkSurface,"#32d45a"};

constexpr const Colors& colors(bool isDark, bool highContrast) {
    if (highContrast) return isDark ? highContrastDark : highContrastLight;
    return isDark ? dark : light;
}

} // namespace mlash::design::ShadowrocketStyle
