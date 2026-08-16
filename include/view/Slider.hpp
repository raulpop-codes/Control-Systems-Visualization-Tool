#ifndef SLIDER_HPP
#define SLIDER_HPP

#include <SFML/Graphics.hpp>

// A simple draggable slider: a horizontal track with a circular handle.
// Click or drag anywhere on the track to move the handle; the value maps
// linearly from [minValue, maxValue] onto the track's pixel width.
class Slider {
private:
    float trackX, trackY, trackWidth;
    double minValue, maxValue, value;
    bool dragging;

    float handleScreenX() const;
    bool setValueFromMouseX(float mouseX);

public:
    Slider(float trackX, float trackY, float trackWidth, double minValue, double maxValue, double initialValue);

    double getValue() const;

    // Returns true if the value changed this frame, so the caller knows
    // whether to recompute anything.
    bool handleEvent(const sf::Event& event, sf::RenderWindow& window);

    void draw(sf::RenderWindow& window, const sf::Font* font) const;
};

#endif
