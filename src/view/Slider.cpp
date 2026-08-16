#include "view/Slider.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

using namespace std;

Slider::Slider(float x, float y, float width, double minVal, double maxVal, double initialVal)
    : trackX(x), trackY(y), trackWidth(width),
      minValue(minVal), maxValue(maxVal), value(initialVal), dragging(false) {}

double Slider::getValue() const {
    return value;
}

float Slider::handleScreenX() const {
    double fraction = (value - minValue) / (maxValue - minValue);
    return trackX + static_cast<float>(fraction) * trackWidth;
}

bool Slider::setValueFromMouseX(float mouseX) {
    double fraction = (mouseX - trackX) / trackWidth;
    fraction = max(0.0, min(1.0, fraction));
    double newValue = minValue + fraction * (maxValue - minValue);
    if(fabs(newValue - value) < 1e-9) return false;
    value = newValue;
    return true;
}

bool Slider::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    if (auto* mousePress = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePress->button == sf::Mouse::Button::Left) {
            float handleX = handleScreenX();
            bool onHandle = fabs(mouse.x - handleX) < 12.0f && fabs(mouse.y - trackY) < 12.0f;
            bool onTrack = mouse.x >= trackX && mouse.x <= trackX + trackWidth && fabs(mouse.y - trackY) < 12.0f;
            if(onHandle || onTrack){
                dragging = true;
                return setValueFromMouseX(mouse.x);
            }
        }
    } else if (auto* mouseRelease = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mouseRelease->button == sf::Mouse::Button::Left) {
            dragging = false;
        }
    } else if (event.is<sf::Event::MouseMoved>() && dragging) {
        return setValueFromMouseX(mouse.x);
    }
    return false;
}

void Slider::draw(sf::RenderWindow& window, const sf::Font* font) const {
    sf::RectangleShape track(sf::Vector2f{trackWidth, 3.0f});
    track.setPosition(sf::Vector2f{trackX, trackY - 1.5f});
    track.setFillColor(sf::Color(100, 100, 110));
    window.draw(track);

    sf::CircleShape handle(8.0f);
    handle.setOrigin(sf::Vector2f{8.0f, 8.0f});
    handle.setPosition(sf::Vector2f{handleScreenX(), trackY});
    handle.setFillColor(sf::Color(80, 160, 240));
    handle.setOutlineColor(sf::Color::White);
    handle.setOutlineThickness(1.0f);
    window.draw(handle);

    if(font != nullptr){
        ostringstream label;
        label << fixed << setprecision(2) << "K = " << value;
        sf::Text text(*font, label.str(), 16);
        text.setPosition(sf::Vector2f{trackX, trackY - 30.0f});
        text.setFillColor(sf::Color::White);
        window.draw(text);
    }
}
