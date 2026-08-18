#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <SFML/Graphics.hpp>
#include <string>

// A plain clickable rectangle with a label -- flat colors, one hover-color
// change, nothing else animated. A "click" is a mouse-down followed by
// mouse-up while still inside the button (dragging off first and
// releasing elsewhere doesn't count), matching ordinary button behaviour.
class Button {
private:
    float x, y, width, height;
    std::string label;
    bool hovered;
    bool pressed;

    bool contains(sf::Vector2f point) const;

public:
    Button(float x, float y, float width, float height, std::string label);

    void setLabel(const std::string& newLabel);

    // Returns true once, on the frame the button is clicked.
    bool handleEvent(const sf::Event& event, sf::RenderWindow& window);

    void draw(sf::RenderWindow& window, const sf::Font* font) const;
};

#endif
