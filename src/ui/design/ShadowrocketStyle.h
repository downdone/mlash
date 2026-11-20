#pragma once

#include "UiDesignTokens.h"

namespace mlash::design::ShadowrocketStyle {

inline constexpr Geometry geometry{30,28,8,8, 14,12,12,216, 42,40,34,36, 36,38,52,350, 12,2,16,76,4};
inline constexpr int narrowSidebarWidth=144;
inline constexpr int narrowSidebarBreakpoint=960;
inline constexpr Typography typography{10.5,10.5,9, 13,11.5,15,13};
inline constexpr int brandPoints=16;
inline constexpr const char* artworkSurface="#F2F2F7";

// Header-only immutable values keep lightweight UI consumers link-independent.
inline constexpr Colors light{
    "#F7F7F7","#F0F0F0","#FFFFFF",
    "#202020","#5B5B5B","#646464","#999999",
    "#DDDDDD","#E7E7E7","#0059B9","#E7F2FF",
    "#0069DC","#FFFFFF","#EAF4FF","#005FC7",
    "#EEEEEE","#E2EBF7","#005BBF",
    "#21683D","#91451F","#AB263B","#777777","#F2F2F7","#21864A"};
inline constexpr Colors dark{
    "#181818","#202020","#2A2A2A",
    "#F7F7F7","#C3C3C3","#B4B4B4","#898989",
    "#505050","#404040","#CDE7FF","#20374F",
    "#65B1FF","#112337","#333B45","#82C1FF",
    "#343434","#3B4653","#8BC4FF",
    "#83DEA2","#FFC1A0","#FFADBA","#929292","#F2F2F7","#4FD183"};
inline constexpr Colors highContrastLight{
    "#FFFFFF","#F3F3F3","#FFFFFF",
    "#141414","#3C3C3C","#333333","#6B6B6B",
    "#666666","#666666","#00448F","#E1EEFF",
    "#00448F","#FFFFFF","#EDEDED","#00458F",
    "#F2F2F2","#D9D9D9","#082C6B",
    "#075D2C","#793212","#8B0628","#505050","#F2F2F7","#075D2C"};
inline constexpr Colors highContrastDark{
    "#000000","#131313","#171717",
    "#FFFFFF","#E4E4E4","#E6E6E6","#8C8C8C",
    "#AAAAAA","#AAAAAA","#E4F2FF","#17344F",
    "#A9D4FF","#0E1E30","#262626","#9FCFFF",
    "#1C1C1C","#333333","#C0DFFF",
    "#9DF6B9","#FFD2BC","#FFD0D8","#AAAAAA","#F2F2F7","#73F0A3"};

constexpr const Colors& colors(bool isDark, bool highContrast) {
    if (highContrast) return isDark ? highContrastDark : highContrastLight;
    return isDark ? dark : light;
}

} // namespace mlash::design::ShadowrocketStyle
