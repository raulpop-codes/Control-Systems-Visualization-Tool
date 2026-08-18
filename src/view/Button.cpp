#include "view/Button.hpp"

Button::Button(float x_, float y_, float w_, float h_, std::string label_)
    : x(x_), y(y_), width(w_), height(h_), label(std::move(label_)),
      hovered(false), pressed(false) {}

void Button::setLabel(const std::string& newLabel) { label = newLabel; }

bool Button::contains(sf::Vector2f point) const {
    return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
}

bool Button::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    hovered = contains(mouse);

    if(const auto* mousePress = event.getIf<sf::Event::MouseButtonPressed>()){
        if(mousePress->button == sf::Mouse::Button::Left && hovered){
            pressed = true;
        }
    } else if(const auto* mouseRelease = event.getIf<sf::Event::MouseButtonReleased>()){
        if(mouseRelease->button == sf::Mouse::Button::Left){
            bool wasClick = pressed && hovered;
            pressed = false;
            if(wasClick) return true;
        }
    }
    return false;
}

void Button::draw(sf::RenderWindow& window, const sf::Font* font) const {
    sf::RectangleShape rect(sf::Vector2f{width, height});
    rect.setPosition({x, y});
    rect.setFillColor(hovered ? sf::Color(70, 120, 190) : sf::Color(55, 90, 140));
    rect.setOutlineColor(sf::Color(160, 190, 230));
    rect.setOutlineThickness(1.0f);
    window.draw(rect);

    if(font != nullptr){
        sf::Text text(*font, label, 16);
        text.setPosition({x + 12.0f, y + height / 2.0f - 10.0f});
        text.setFillColor(sf::Color::White);
        window.draw(text);
    }
}
