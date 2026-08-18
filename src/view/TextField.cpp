#include "view/TextField.hpp"
#include <string>

using namespace std;

TextField::TextField(float x_, float y_, float w_, float h_, string label_, string initialValue)
    : x(x_), y(y_), width(w_), height(h_), label(std::move(label_)),
      value(std::move(initialValue)), focused(false) {}

const string& TextField::getValue() const { return value; }
void TextField::setValue(const string& newValue) { value = newValue; }

double TextField::getValueAsDouble(double fallback) const {
    if(value.empty()) return fallback;
    try {
        size_t consumed = 0;
        double result = stod(value, &consumed);
        if(consumed != value.size()) return fallback;
        return result;
    } catch(...) {
        return fallback;
    }
}

bool TextField::isFocused() const { return focused; }
void TextField::focus() { focused = true; }
void TextField::unfocus() { focused = false; }

bool TextField::contains(sf::Vector2f point) const {
    return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
}

bool TextField::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if(const auto* mousePress = event.getIf<sf::Event::MouseButtonPressed>()){
        if(mousePress->button == sf::Mouse::Button::Left){
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if(contains(mouse)){
                focused = true;
                return true;
            }
        }
        return false;
    }

    if(!focused) return false;

    if(const auto* textEntered = event.getIf<sf::Event::TextEntered>()){
        char32_t unicode = textEntered->unicode;
        if(unicode == 8){ // backspace
            if(!value.empty()) value.pop_back();
        } else if(unicode == '.' || unicode == '-' || unicode == 'e' || unicode == 'E'
                  || (unicode >= '0' && unicode <= '9')){
            if(value.size() < 24) value.push_back(static_cast<char>(unicode));
        }
    }
    return false;
}

void TextField::draw(sf::RenderWindow& window, const sf::Font* font) const {
    sf::RectangleShape box(sf::Vector2f{width, height});
    box.setPosition({x, y});
    box.setFillColor(sf::Color(25, 25, 32));
    box.setOutlineColor(focused ? sf::Color(90, 160, 240) : sf::Color(90, 90, 100));
    box.setOutlineThickness(focused ? 2.0f : 1.0f);
    window.draw(box);

    if(font != nullptr){
        if(!label.empty()){
            sf::Text labelText(*font, label, 13);
            labelText.setPosition({x, y - 17.0f});
            labelText.setFillColor(sf::Color(170, 170, 180));
            window.draw(labelText);
        }

        string shown = value + (focused ? "|" : "");
        sf::Text valueText(*font, shown, 16);
        valueText.setPosition({x + 6.0f, y + height / 2.0f - 10.0f});
        valueText.setFillColor(sf::Color::White);
        window.draw(valueText);
    }
}
