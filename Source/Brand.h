#pragma once
#include <juce_graphics/juce_graphics.h>

// ============================================================================
//  BRAND / LOOK  -  change colours and text here, nothing else needs touching.
//  (The plugin NAME and VERSION live in CMakeLists.txt, top of the file.)
// ============================================================================
namespace sty
{
    const juce::Colour bg      { 0xff05020c };   // window background
    const juce::Colour panel   { 0xff160a2a };   // buttons / dropdowns
    const juce::Colour accent  { 0xff9d3cff };   // electric purple
    const juce::Colour neon    { 0xffff4fd8 };   // corrected pitch line (hot pink)
    const juce::Colour cyan    { 0xff35f0ff };   // highlights / value text
    const juce::Colour rawLine { 0xff7f8bb0 };   // original pitch line
    const juce::Colour text    { 0xfff4ecff };

    inline const char* tagline() { return "PITCH LOCK  /  CRUSHAUDIO"; }
}
