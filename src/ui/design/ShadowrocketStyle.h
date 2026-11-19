#pragma once

#include "UiDesignTokens.h"

namespace mlash::design::ShadowrocketStyle {

inline constexpr Geometry geometry{30,28,8,8, 14,12,12,216, 42,40,34,36, 36,38,52,350, 12,2,16,76,4};
inline constexpr Typography typography{9.5,10.0,0.87,8.25, 15,10,14,12};
inline constexpr const char* artworkSurface="#F2F2F7";

// Header-only immutable values keep lightweight UI consumers link-independent.
inline constexpr Colors light{
    "#F4F5F8","#F3F4F6","#FFFFFF",
    "#1B2430","#526174","#5D6A7B","#8E98A5",
    "#DDE2E9","#E6EAF0","#075DBB","#E4EFFC",
    "#075DBB","#FFFFFF","#E9EDF3","#075DBB",
    "#F2F4F7","#E1E7EF","#004B9A",
    "#126D43","#855100","#B42332","#8894A5","#F4F5F8"};
inline constexpr Colors dark{
    "#111319","#1A1D24","#22262F",
    "#F2F4F7","#C5CDD8","#AEB8C6","#7F8998",
    "#38414F","#323A46","#7CB6FF","#1D3C61",
    "#1767BC","#FFFFFF","#2D3540","#7CB6FF",
    "#2A303A","#343D49","#0B529A",
    "#71D8A0","#F2C374","#FF9CA5","#858F9D","#F4F5F8"};
inline constexpr Colors highContrastLight{
    "#FFFFFF","#FFFFFF","#FFFFFF",
    "#000000","#1A1A1A","#333333","#6B6B6B",
    "#000000","#000000","#0B3D91","#DCE6F7",
    "#0B3D91","#FFFFFF","#EDEDED","#0B63CE",
    "#F2F2F2","#D9D9D9","#082C6B",
    "#00552E","#6B3A00","#A30018","#4A4A4A","#F2F2F7"};
inline constexpr Colors highContrastDark{
    "#000000","#000000","#101010",
    "#FFFFFF","#FFFFFF","#E6E6E6","#8C8C8C",
    "#FFFFFF","#FFFFFF","#FFD60A","#3A2F00",
    "#FFD60A","#000000","#262626","#FFD60A",
    "#1C1C1C","#333333","#FFE44D",
    "#5CFFA8","#FFD60A","#FF8A9A","#FFFFFF","#F2F2F7"};

constexpr const Colors& colors(bool isDark, bool highContrast) {
    if (highContrast) return isDark ? highContrastDark : highContrastLight;
    return isDark ? dark : light;
}

} // namespace mlash::design::ShadowrocketStyle
