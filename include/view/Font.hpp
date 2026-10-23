#ifndef FONT_HPP
#define FONT_HPP

#include <SFML/Graphics.hpp>
#include <iostream>

// Every window needs the same font and SFML has no built-in default, so this
// looks for a system font in the usual place on each platform. inline so it
// can live in a header without a .cpp of its own.
inline sf::Font loadFont() {
    sf::Font font;
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    for(const char* path : candidates){
        if(font.openFromFile(path)) return font;
    }
    std::cerr << "Warning: no font found, labels will not be drawn.\n";
    return font;
}

#endif
