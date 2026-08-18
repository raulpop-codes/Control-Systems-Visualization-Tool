#ifndef TEXT_FIELD_HPP
#define TEXT_FIELD_HPP

#include <SFML/Graphics.hpp>
#include <string>

// A single-line number entry box. Click it to focus (the app is
// responsible for unfocusing every other field when that happens -- see
// LauncherWindow.cpp), then type: digits, '.', '-', and 'e'/'E' are
// accepted (enough for plain and scientific-notation numbers), Backspace
// deletes the last character. No blinking cursor or other animation --
// just a static "|" shown while focused.
class TextField {
private:
    float x, y, width, height;
    std::string label; // shown above the box; leave empty for none
    std::string value;
    bool focused;

    bool contains(sf::Vector2f point) const;

public:
    TextField(float x, float y, float width, float height, std::string label, std::string initialValue = "");

    const std::string& getValue() const;
    void setValue(const std::string& newValue);
    // Parses the current text as a number; returns fallback if it's empty
    // or not a valid number. See LauncherWindow.cpp's parseStrict() for a
    // version that raises instead of silently falling back, used right
    // before building a system from the entered values.
    double getValueAsDouble(double fallback = 0.0) const;

    bool isFocused() const;
    void focus();
    void unfocus();

    // Click-to-focus, and while focused, absorbs typed characters. Returns
    // true on the frame this field was just clicked.
    bool handleEvent(const sf::Event& event, sf::RenderWindow& window);

    void draw(sf::RenderWindow& window, const sf::Font* font) const;
};

#endif
