#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Audio.hpp>
#include <array>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <memory>
#include <fstream>
#include <cmath>
#include <string>
#include <SFML/System/Vector2.hpp>
#include <algorithm>

using namespace sf;

const int BLOCK_SIZE = 35;
const int GRID_WIDTH = 10;
const int GRID_HEIGHT = 25;
const int SIDEBAR_WIDTH = 400;
const int SCREEN_WIDTH = GRID_WIDTH * BLOCK_SIZE + SIDEBAR_WIDTH;
const int SCREEN_HEIGHT = GRID_HEIGHT * BLOCK_SIZE;
const float INITIAL_FALL_TIME = 0.5f;

enum class GameState {
    Menu,
    Playing,
    Paused,
    GameOver,
    HowToPlay,
    HighScores
};

class ParticleSystem {
private:
    struct Particle {
        sf::Vector2f position;
        sf::Vector2f velocity;
        sf::Color color;
        float lifetime;
    };

    std::vector<Particle> particles;
    sf::VertexArray vertices;
    float lifetime;

public:
    ParticleSystem() : vertices(sf::Points), lifetime(1.0f) {}

    void addParticle(const sf::Vector2f& position, const sf::Color& color) {
        Particle particle;
        particle.position = position;
        particle.velocity = sf::Vector2f(
            (rand() % 200 - 100) * 0.1f,
            (rand() % 200 - 100) * 0.1f
        );
        particle.color = color;
        particle.lifetime = lifetime;
        particles.push_back(particle);
    }

    void update(float deltaTime) {
        for (size_t i = 0; i < particles.size();) {
            Particle& p = particles[i];
            p.lifetime -= deltaTime;

            if (p.lifetime <= 0) {
                particles.erase(particles.begin() + i);
                continue;
            }

            p.position += p.velocity * deltaTime;
            p.color.a = static_cast<sf::Uint8>(255 * (p.lifetime / lifetime));

            i++;
        }
    }

    void draw(sf::RenderTarget& target) {
        vertices.clear();
        for (const auto& particle : particles) {
            sf::Vertex vertex(particle.position, particle.color);
            vertices.append(vertex);
        }
        target.draw(vertices);
    }
};

const std::array<std::vector<std::vector<std::vector<int>>>, 7> SHAPES = { {
    {{{1, 1, 1, 1}}, {{1}, {1}, {1}, {1}}},
    {{{1, 0, 0}, {1, 1, 1}}, {{1, 1}, {1, 0}, {1, 0}}, {{1, 1, 1}, {0, 0, 1}}, {{0, 1}, {0, 1}, {1, 1}}},
    {{{0, 0, 1}, {1, 1, 1}}, {{1, 0}, {1, 0}, {1, 1}}, {{1, 1, 1}, {1, 0, 0}}, {{1, 1}, {0, 1}, {0, 1}}},
    {{{1, 1}, {1, 1}}},
    {{{0, 1, 1}, {1, 1, 0}}, {{1, 0}, {1, 1}, {0, 1}}},
    {{{0, 1, 0}, {1, 1, 1}}, {{1, 0}, {1, 1}, {1, 0}}, {{1, 1, 1}, {0, 1, 0}}, {{0, 1}, {1, 1}, {0, 1}}},
    {{{1, 1, 0}, {0, 1, 1}}, {{0, 1}, {1, 1}, {1, 0}}}
} };

const std::array<sf::Color, 7> COLORS = {
    sf::Color(0, 255, 255),
    sf::Color(0, 0, 255),
    sf::Color(255, 165, 0),
    sf::Color(255, 255, 0),
    sf::Color(0, 255, 0),
    sf::Color(255, 0, 255),
    sf::Color(255, 0, 0)
};

const sf::Color BACKGROUND_COLOR(30, 30, 30);

struct Piece {
    int x, y;
    int rotation;
    int type;
    sf::Color color;

    Piece() : x(4), y(0), rotation(0), type(rand() % 7) {
        color = COLORS[type];
    }

    std::vector<sf::Vector2i> getBlocks() const {
        std::vector<sf::Vector2i> blocks;
        const auto& shape = SHAPES[type][rotation % SHAPES[type].size()];

        for (size_t i = 0; i < shape.size(); ++i) {
            for (size_t j = 0; j < shape[i].size(); ++j) {
                if (shape[i][j]) {
                    blocks.emplace_back(x + j, y + i);
                }
            }
        }
        return blocks;
    }
};

bool isValidPosition(const Piece& piece, const std::array<std::array<int, GRID_WIDTH>, GRID_HEIGHT>& grid) {
    for (const auto& block : piece.getBlocks()) {
        if (block.x < 0 || block.x >= GRID_WIDTH || block.y >= GRID_HEIGHT) return false;
        if (block.y >= 0 && grid[block.y][block.x]) return false;
    }
    return true;
}

class Game {
private:
    void drawGridBackground() {
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                sf::RectangleShape cell(sf::Vector2f(BLOCK_SIZE - 2, BLOCK_SIZE - 2));
                cell.setPosition(static_cast<float>(x * BLOCK_SIZE + 1),
                    static_cast<float>(y * BLOCK_SIZE + 1));
                cell.setFillColor(BACKGROUND_COLOR);
                cell.setOutlineThickness(1);
                cell.setOutlineColor(sf::Color(70, 70, 70));
                window.draw(cell);

                if (grid[y][x]) {
                    sf::RectangleShape block(sf::Vector2f(BLOCK_SIZE - 2, BLOCK_SIZE - 2));
                    block.setPosition(static_cast<float>(x * BLOCK_SIZE + 1),
                        static_cast<float>(y * BLOCK_SIZE + 1));
                    block.setTexture(&blockTextures[colorsGrid[y][x] - 1]);
                    window.draw(block);
                }
            }
        }
    }

    void handleGameOverEvents(const sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            switch (event.key.code) {
            case sf::Keyboard::Space:
                initializeGame();
                state = GameState::Playing;
                break;
            case sf::Keyboard::Escape:
                state = GameState::Menu;
                break;
            default:
                break;
            }
        }
    }

    sf::Text gameOverText;

    void handlePausedEvents(const sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            switch (event.key.code) {
            case sf::Keyboard::Escape:
                state = GameState::Playing;
                backgroundMusic.play();
                break;
            case sf::Keyboard::Q:
                state = GameState::Menu;
                break;
            default:
                break;
            }
        }
    }

    sf::RenderWindow window;
    std::array<std::array<int, GRID_WIDTH>, GRID_HEIGHT> grid;
    std::array<std::array<int, GRID_WIDTH>, GRID_HEIGHT> colorsGrid;
    Piece currentPiece;
    Piece nextPiece;
    Piece holdPiece;
    bool canHold;

    sf::Font mainFont;
    sf::Font titleFont;
    sf::Texture backgroundTexture;
    sf::Texture logoTexture;
    sf::Texture blockTextures[7];
    sf::Sprite backgroundSprite;
    sf::Sprite logoSprite;

    sf::SoundBuffer rotateBuffer;
    sf::SoundBuffer clearBuffer;
    sf::SoundBuffer dropBuffer;
    sf::Sound rotateSound;
    sf::Sound clearSound;
    sf::Sound dropSound;
    sf::Music backgroundMusic;

    GameState state;
    int score;
    int level;
    int linesCleared;
    int highScore;
    sf::Clock clock;
    float fallTime;
    float currentTime;
    bool isGameOver;

    ParticleSystem particles;
    std::vector<sf::RectangleShape> ghostPiece;
    float flashEffect;

    sf::RectangleShape sidebar;
    std::vector<sf::Text> menuOptions;
    int selectedOption;

    void renderHowToPlay() {
        sf::RectangleShape overlay(sf::Vector2f(SCREEN_WIDTH, SCREEN_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 230));
        window.draw(overlay);

        sf::Text title("HOW TO PLAY", titleFont, 50);
        centerText(title, SCREEN_HEIGHT * 0.15f);
        title.setFillColor(sf::Color::Yellow);
        window.draw(title);

        std::vector<std::string> instructions = {
            "CONTROLS:",
            "Arrow keys Left/Right      Move piece left/right",
            "Arrow key Up      Rotate piece",
            "Arrow key Down      Soft drop",
            "C :     Hold piece",
            "ESC :     Pause game",
            "",
            "SCORING:",
            "1 line: 100      level",
            "2 lines: 300     level",
            "3 lines: 500     level",
            "4 lines: 800     level"
        };

        float startY = SCREEN_HEIGHT * 0.3f;
        for (const auto& line : instructions) {
            sf::Text text(line, mainFont, 25);
            centerText(text, startY);
            window.draw(text);
            startY += 35;
        }

        sf::Text backText("Press ESC to return", mainFont, 20);
        centerText(backText, SCREEN_HEIGHT * 0.9f);
        window.draw(backText);
    }

    void renderHighScores() {
        sf::RectangleShape overlay(sf::Vector2f(SCREEN_WIDTH, SCREEN_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 230));
        window.draw(overlay);

        sf::Text title("HIGH SCORES", titleFont, 50);
        centerText(title, SCREEN_HEIGHT * 0.15f);
        title.setFillColor(sf::Color::Yellow);
        window.draw(title);

        std::vector<int> topScores = loadTopScores();
        float startY = SCREEN_HEIGHT * 0.3f;

        for (size_t i = 0; i < topScores.size(); ++i) {
            std::stringstream ss;
            ss << (i + 1) << ". " << topScores[i];
            sf::Text scoreText(ss.str(), mainFont, 30);
            centerText(scoreText, startY);
            window.draw(scoreText);
            startY += 50;
        }

        sf::Text backText("Press ESC to return", mainFont, 20);
        centerText(backText, SCREEN_HEIGHT * 0.9f);
        window.draw(backText);
    }

    std::vector<int> loadTopScores() {
        std::vector<int> scores;
        std::ifstream file("highscores.txt");
        int score;
        while (file >> score && scores.size() < 5) {
            scores.push_back(score);
        }
        file.close();

        while (scores.size() < 5) {
            scores.push_back(0);
        }
        return scores;
    }

    void saveTopScores(int newScore) {
        std::vector<int> scores = loadTopScores();
        scores.push_back(newScore);
        std::sort(scores.begin(), scores.end(), std::greater<int>());
        scores.resize(5);

        std::ofstream file("highscores.txt");
        for (int score : scores) {
            file << score << "\n";
        }
        file.close();
    }
void setupSpritesAndUI() {
    backgroundSprite.setTexture(backgroundTexture);
    logoSprite.setTexture(logoTexture);

    float logoScale = 0.7f;
    logoSprite.setScale(logoScale, logoScale);

    sidebar.setSize(sf::Vector2f(SIDEBAR_WIDTH, SCREEN_HEIGHT));
    sidebar.setPosition(GRID_WIDTH * BLOCK_SIZE, 0);
    sidebar.setFillColor(sf::Color(30, 30, 30, 230));

    menuOptions = {
        createText("Start Game", 40),
        createText("How to Play", 40),
        createText("High Scores", 40),
        createText("Exit", 40)
    };

    float menuY = SCREEN_HEIGHT / 2;
    for (size_t i = 0; i < menuOptions.size(); ++i) {
        centerText(menuOptions[i], menuY + i * 60);
    }
}


public:
   Game() : window(sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), "TETRIS",
    sf::Style::Titlebar | sf::Style::Close),
    score(0), level(1), linesCleared(0), fallTime(INITIAL_FALL_TIME),
    currentTime(0.0f), isGameOver(false), canHold(true), state(GameState::Menu),
    selectedOption(0),
    flashEffect(0.0f) {

    window.setFramerateLimit(60);
    initializeResources();
    initializeGame();
    loadHighScore();
}
private:
    void initializeResources() {
    try {
        if (!mainFont.loadFromFile("resources/main_font.ttf")) {
            throw std::runtime_error("Failed to load main_font.ttf");
        }
        if (!titleFont.loadFromFile("resources/title_font.ttf")) {
            throw std::runtime_error("Failed to load title_font.ttf");
        }

        if (!backgroundTexture.loadFromFile("resources/background.png")) {
            throw std::runtime_error("Failed to load background.png");
        }
        if (!logoTexture.loadFromFile("resources/university_logo.png")) {
            throw std::runtime_error("Failed to load university_logo.png");
        }

        for (int i = 0; i < 7; i++) {
            std::string filename = "resources/block" + std::to_string(i) + ".png";
            if (!blockTextures[i].loadFromFile(filename)) {
                throw std::runtime_error("Failed to load " + filename);
            }
        }

        if (!rotateBuffer.loadFromFile("resources/rotate.wav")) {
            throw std::runtime_error("Failed to load rotate.wav");
        }
        if (!clearBuffer.loadFromFile("resources/clear.wav")) {
            throw std::runtime_error("Failed to load clear.wav");
        }
        if (!dropBuffer.loadFromFile("resources/drop.wav")) {
            throw std::runtime_error("Failed to load drop.wav");
        }
        if (!backgroundMusic.openFromFile("resources/background_music.ogg")) {
            throw std::runtime_error("Failed to load background_music.ogg");
        }

        rotateSound.setBuffer(rotateBuffer);
        clearSound.setBuffer(clearBuffer);
        dropSound.setBuffer(dropBuffer);
        backgroundMusic.setLoop(true);
        backgroundMusic.setVolume(40);

        setupSpritesAndUI();
    }
    catch (const std::runtime_error& e) {
        throw;
    }
}
    sf::Text createText(const std::string& content, unsigned int size) {
        sf::Text text;
        text.setFont(mainFont);
        text.setString(content);
        text.setCharacterSize(size);
        text.setFillColor(sf::Color::White);
        return text;
    }

    void centerText(sf::Text& text, float y) {
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin(bounds.width / 2, bounds.height / 2);
        text.setPosition(SCREEN_WIDTH / 2, y);
    }
    void initializeGame() {
    grid.fill({});
    colorsGrid.fill({});
    score = 0;
    level = 1;
    linesCleared = 0;
    fallTime = INITIAL_FALL_TIME;
    currentTime = 0.0f;
    isGameOver = false;
    canHold = true;
    currentPiece = Piece();
    nextPiece = Piece();
    holdPiece = Piece();
    holdPiece.type = -1;
    updateGhostPiece();
    flashEffect = 0.0f;
}

    void updateGhostPiece() {
        ghostPiece.clear();
        Piece ghost = currentPiece;
        while (isValidPosition(ghost, grid)) {
            ghost.y++;
        }
        ghost.y--;

        for (const auto& block : ghost.getBlocks()) {
            if (block.y >= 0) {
                sf::RectangleShape rect(sf::Vector2f(BLOCK_SIZE - 2, BLOCK_SIZE - 2));
                rect.setPosition(block.x * BLOCK_SIZE + 1, block.y * BLOCK_SIZE + 1);
                rect.setFillColor(sf::Color(currentPiece.color.r, currentPiece.color.g,
                    currentPiece.color.b, 50));
                ghostPiece.push_back(rect);
            }
        }
    }

    void loadHighScore() {
        std::ifstream file("highscore.txt");
        if (file.is_open()) {
            file >> highScore;
            file.close();
        }
        else {
            highScore = 0;
        }
    }

    void saveHighScore() {
        if (score > highScore) {
            highScore = score;
            std::ofstream file("highscore.txt");
            if (file.is_open()) {
                file << highScore;
                file.close();
            }
        }
    }

    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            switch (state) {
            case GameState::Menu:
                handleMenuEvents(event);
                break;
            case GameState::Playing:
                handleGameEvents(event);
                break;
            case GameState::Paused:
                handlePausedEvents(event);
                break;
            case GameState::GameOver:
                handleGameOverEvents(event);
                break;
            case GameState::HowToPlay:
            case GameState::HighScores:
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    state = GameState::Menu;
                }
                break;
            }
        }
    }


    void handleMenuEvents(const sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            switch (event.key.code) {
            case sf::Keyboard::Up:
                selectedOption = (selectedOption - 1 + menuOptions.size()) % menuOptions.size();
                break;
            case sf::Keyboard::Down:
                selectedOption = (selectedOption + 1) % menuOptions.size();
                break;
            case sf::Keyboard::Return:
                executeMenuOption();
                break;
            default:
                break;
            }
        }
    }

    void executeMenuOption() {
        switch (selectedOption) {
        case 0:
            initializeGame();
            state = GameState::Playing;
            backgroundMusic.play();
            break;
        case 1:
            state = GameState::HowToPlay;
            break;
        case 2:
            state = GameState::HighScores;
            break;
        case 3:
            window.close();
            break;
        }
    }

    void handleGameEvents(const sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            Piece temp = currentPiece;
            switch (event.key.code) {
            case sf::Keyboard::Left:
                temp.x--;
                break;
            case sf::Keyboard::Right:
                temp.x++;
                break;
            case sf::Keyboard::Down:
                temp.y++;
                break;
            case sf::Keyboard::Up:
                temp.rotation = (temp.rotation + 1) % SHAPES[temp.type].size();
                if (isValidPosition(temp, grid)) {
                    rotateSound.play();
                }
                break;
            case sf::Keyboard::Space:
                hardDrop();
                break;
            case sf::Keyboard::C:
                handleHoldPiece();
                break;
            case sf::Keyboard::Escape:
                state = GameState::Paused;
                backgroundMusic.pause();
                break;
            default:
                break;
            }
            if (isValidPosition(temp, grid)) {
                currentPiece = temp;
                updateGhostPiece();
            }
        }
    }

    void handleHoldPiece() {
        if (!canHold) return;

        if (holdPiece.type == -1) {
            holdPiece = currentPiece;
            currentPiece = nextPiece;
            nextPiece = Piece();
        }
        else {
            std::swap(holdPiece, currentPiece);
            currentPiece.x = 4;
            currentPiece.y = 0;
            currentPiece.rotation = 0;
        }

        canHold = false;
        updateGhostPiece();
    }

    void hardDrop() {
        while (isValidPosition(currentPiece, grid)) {
            currentPiece.y++;
        }
        currentPiece.y--;
        lockPiece();
        dropSound.play();
        currentTime = 0.0f;
    }

    void lockPiece() {
    for (const auto& block : currentPiece.getBlocks()) {
        if (block.y >= 0) {
            grid[block.y][block.x] = 1;
            colorsGrid[block.y][block.x] = currentPiece.type + 1;
            particles.addParticle(
                sf::Vector2f(block.x * BLOCK_SIZE + BLOCK_SIZE / 2,
                    block.y * BLOCK_SIZE + BLOCK_SIZE / 2),
                currentPiece.color
            );
        }
    }

        currentPiece = nextPiece;
        nextPiece = Piece();
        canHold = true;
        if (!isValidPosition(currentPiece, grid)) {
            gameOver();
        }

        checkRows();
        updateGhostPiece();
    }

    void gameOver() {
        state = GameState::GameOver;
        saveHighScore();
        saveTopScores(score);
        backgroundMusic.stop();
    }

    void checkRows() {
        int rowsCleared = 0;
        for (int y = GRID_HEIGHT - 1; y >= 0; --y) {
            bool full = true;
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (!grid[y][x]) {
                    full = false;
                    break;
                }
            }

            if (full) {
                rowsCleared++;
                for (int x = 0; x < GRID_WIDTH; x++) {
                    particles.addParticle(
                        sf::Vector2f(x * BLOCK_SIZE + BLOCK_SIZE / 2, y * BLOCK_SIZE + BLOCK_SIZE / 2),
                        COLORS[colorsGrid[y][x] - 1]
                    );
                }

                for (int row = y; row > 0; --row) {
                    grid[row] = grid[row - 1];
                    colorsGrid[row] = colorsGrid[row - 1];
                }
                grid[0].fill(0);
                colorsGrid[0].fill(0);
                ++y;
            }
        }

        if (rowsCleared > 0) {
            clearSound.play();
            score += calculateScore(rowsCleared);
            linesCleared += rowsCleared;
            level = 1 + (linesCleared / 10);
            fallTime = std::max(0.1f, INITIAL_FALL_TIME - (level - 1) * 0.05f);
            flashEffect = 0.5f;
        }
    }

    int calculateScore(int rows) {
        switch (rows) {
        case 1: return 100 * level;
        case 2: return 300 * level;
        case 3: return 500 * level;
        case 4: return 800 * level;
        default: return 0;
        }
    }

    void update() {
        float deltaTime = clock.restart().asSeconds();

        switch (state) {
        case GameState::Playing:
            updateGame(deltaTime);
            break;
        case GameState::Menu:
            updateMenu(deltaTime);
            break;
        case GameState::GameOver:
            updateGameOver(deltaTime);
            break;
        default:
            break;
        }

        particles.update(deltaTime);
        if (flashEffect > 0) {
            flashEffect -= deltaTime;
        }
    }

    void updateGame(float deltaTime) {
        currentTime += deltaTime;
        if (currentTime >= fallTime) {
            currentTime = 0.0f;
            Piece temp = currentPiece;
            temp.y++;
            if (isValidPosition(temp, grid)) {
                currentPiece = temp;
                updateGhostPiece();
            }
            else {
                lockPiece();
            }
        }
    }

    void updateMenu(float deltaTime) {
    static float pulseTime = 0;
    pulseTime += deltaTime * 2;

    for (size_t i = 0; i < menuOptions.size(); ++i) {
        if (i == selectedOption) {
            float pulse = (sin(pulseTime) + 1) / 2;
            menuOptions[i].setFillColor(sf::Color(255, 255, 0,
                static_cast<sf::Uint8>(155 + 100 * pulse)));
            menuOptions[i].setScale(1.1f, 1.1f);
        }
        else {
            menuOptions[i].setFillColor(sf::Color::White);
            menuOptions[i].setScale(1.0f, 1.0f);
        }
    }
}

    void updateGameOver(float deltaTime) {
        static float textPulse = 0;
        textPulse += deltaTime * 2;
        gameOverText.setFillColor(sf::Color(255, 0, 0,
            static_cast<sf::Uint8>(155 + 100 * ((sin(textPulse) + 1) / 2))));
    }

    void render() {
    window.clear(BACKGROUND_COLOR);
    window.draw(backgroundSprite);

    switch (state) {
        case GameState::Playing:
            drawGridBackground();
            for (const auto& block : ghostPiece) {
                window.draw(block);
            }
            for (const auto& block : currentPiece.getBlocks()) {
                if (block.y >= 0) {
                    sf::RectangleShape rect(sf::Vector2f(BLOCK_SIZE - 2, BLOCK_SIZE - 2));
                    rect.setPosition(block.x * BLOCK_SIZE + 1, block.y * BLOCK_SIZE + 1);
                    rect.setTexture(&blockTextures[currentPiece.type]);
                    window.draw(rect);
                }
            }
            drawUI();
            particles.draw(window);
            break;
        case GameState::Menu:
            renderMenu();
            break;
        case GameState::Paused:
            drawGridBackground();
            drawUI();
            renderPauseScreen();
            break;
        case GameState::GameOver:
            drawGridBackground();
            drawUI();
            renderGameOver();
            break;
        case GameState::HowToPlay:
            renderHowToPlay();
            break;
        case GameState::HighScores:
            renderHighScores();
            break;
    }

    window.display();
}

    void renderGame() {
       window.clear(BACKGROUND_COLOR);
        window.draw(backgroundSprite);

        switch (state) {
        case GameState::Playing:
            renderGame();
            break;
        case GameState::Menu:
            renderMenu();
            break;
        case GameState::Paused:
            renderGame();
            renderPauseScreen();
            break;
        case GameState::GameOver:
            renderGame();
            renderGameOver();
            break;
        case GameState::HowToPlay:
            renderHowToPlay();
            break;
        case GameState::HighScores:
            renderHighScores();
            break;
        }

        window.display();
    }

    void renderMenu() {
    logoSprite.setPosition(
        (SCREEN_WIDTH - logoSprite.getGlobalBounds().width) / 2,
        20
    );
    window.draw(logoSprite);

    sf::Text creditText("Made by Najaf Ali", mainFont, 36);
    creditText.setPosition(
        (SCREEN_WIDTH - creditText.getGlobalBounds().width) / 2,
        logoSprite.getPosition().y + logoSprite.getGlobalBounds().height + 14
    );
    creditText.setFillColor(sf::Color::White);
    window.draw(creditText);

    sf::Text mainTitle("TETRIS", titleFont, 150);
    mainTitle.setStyle(sf::Text::Bold);
    centerText(mainTitle, SCREEN_HEIGHT * 0.34f);
    mainTitle.setFillColor(sf::Color::Yellow);
    window.draw(mainTitle);

    for (const auto& option : menuOptions) {
        window.draw(option);
    }
}

    void renderPauseScreen() {
        sf::RectangleShape overlay(sf::Vector2f(SCREEN_WIDTH, SCREEN_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 128));
        window.draw(overlay);

        sf::Text pauseText("PAUSED", titleFont, 60);
        centerText(pauseText, SCREEN_HEIGHT / 2);
        window.draw(pauseText);

        sf::Text resumeText("Press ESC to resume", mainFont, 30);
        centerText(resumeText, SCREEN_HEIGHT / 2 + 80);
        window.draw(resumeText);
    }

    void renderGameOver() {
        sf::RectangleShape overlay(sf::Vector2f(SCREEN_WIDTH, SCREEN_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 200));
        window.draw(overlay);

        sf::Text gameOverTitle("GAME OVER", titleFont, 70);
        centerText(gameOverTitle, SCREEN_HEIGHT * 0.3f);
        gameOverTitle.setFillColor(sf::Color::Red);
        window.draw(gameOverTitle);

        std::stringstream ss;
        ss << "Final Score: " << score << "\n";
        ss << "High Score: " << highScore << "\n";
        ss << "Lines Cleared: " << linesCleared << "\n";
        ss << "Level Reached: " << level;

        sf::Text statsText(ss.str(), mainFont, 30);
        centerText(statsText, SCREEN_HEIGHT * 0.5f);
        window.draw(statsText);

        sf::Text restartText("Press SPACE to restart\nPress ESC to return to menu", mainFont, 25);
        centerText(restartText, SCREEN_HEIGHT * 0.7f);
        window.draw(restartText);
    }

    void drawUI() {
    window.draw(sidebar);

    float sidebarCenterX = GRID_WIDTH * BLOCK_SIZE + (SIDEBAR_WIDTH / 2);

    float logoScale = 0.7f;
    logoSprite.setScale(logoScale, logoScale);
    logoSprite.setPosition(
        sidebarCenterX - (logoSprite.getGlobalBounds().width / 2),
        20
    );
    window.draw(logoSprite);

    sf::Text creditText("Made By Najaf Ali", mainFont, 24);
    sf::FloatRect creditBounds = creditText.getLocalBounds();
    creditText.setOrigin(creditBounds.width / 2, creditBounds.height / 2);
    creditText.setPosition(
        sidebarCenterX,
        logoSprite.getPosition().y + logoSprite.getGlobalBounds().height + 20
    );
    creditText.setFillColor(sf::Color::White);
    window.draw(creditText);

    std::vector<std::pair<std::string, std::string>> stats = {
        {"SCORE", std::to_string(score)},
        {"LEVEL", std::to_string(level)},
        {"LINES", std::to_string(linesCleared)},
        {"HIGH SCORE", std::to_string(highScore)}
    };

    float startY = creditText.getPosition().y + creditText.getGlobalBounds().height + 40;
    for (const auto& stat : stats) {
        sf::Text labelText(stat.first, mainFont, 20);
        sf::Text valueText(stat.second, mainFont, 30);

        sf::FloatRect labelBounds = labelText.getLocalBounds();
        sf::FloatRect valueBounds = valueText.getLocalBounds();

        labelText.setOrigin(labelBounds.width / 2, labelBounds.height / 2);
        valueText.setOrigin(valueBounds.width / 2, valueBounds.height / 2);

        labelText.setPosition(sidebarCenterX, startY);
        valueText.setPosition(sidebarCenterX, startY + 25);

        labelText.setFillColor(sf::Color(200, 200, 200));
        valueText.setFillColor(sf::Color::White);

        window.draw(labelText);
        window.draw(valueText);
        startY += 80;
    }

    drawHoldPiece(startY);

    drawNextPiece(startY + 120);
}
    void drawHoldPiece(float y) {
        sf::Text holdText("HOLD", mainFont, 20);
        holdText.setPosition(GRID_WIDTH * BLOCK_SIZE + 20, y);
        holdText.setFillColor(sf::Color(200, 200, 200));
        window.draw(holdText);

        if (holdPiece.type != -1) {
            const auto& shape = SHAPES[holdPiece.type][0];
            for (size_t i = 0; i < shape.size(); ++i) {
                for (size_t j = 0; j < shape[i].size(); ++j) {
                    if (shape[i][j]) {
                        sf::RectangleShape block(sf::Vector2f(BLOCK_SIZE - 2, BLOCK_SIZE - 2));
                        block.setPosition(
                            GRID_WIDTH * BLOCK_SIZE + 20 + j * BLOCK_SIZE,
                            y + 30 + i * BLOCK_SIZE
                        );
                        block.setTexture(&blockTextures[holdPiece.type]);
                        window.draw(block);
                    }
                }
            }
        }
    }

    void drawNextPiece(float y) {
        sf::Text nextText("NEXT", mainFont, 20);
        nextText.setPosition(GRID_WIDTH * BLOCK_SIZE + 20, y);
        nextText.setFillColor(sf::Color(200, 200, 200));
        window.draw(nextText);

        const auto& shape = SHAPES[nextPiece.type][0];
        for (size_t i = 0; i < shape.size(); ++i) {
            for (size_t j = 0; j < shape[i].size(); ++j) {
                if (shape[i][j]) {
                    sf::RectangleShape block(sf::Vector2f(BLOCK_SIZE - 2, BLOCK_SIZE - 2));
                    block.setPosition(
                        GRID_WIDTH * BLOCK_SIZE + 20 + j * BLOCK_SIZE,
                        y + 30 + i * BLOCK_SIZE
                    );
                    block.setTexture(&blockTextures[nextPiece.type]);
                    window.draw(block);
                }
            }
        }
    }

public:
    void run() {
        while (window.isOpen()) {
            handleEvents();
            update();
            render();
        }
    }
};

int main() {
    srand(static_cast<unsigned>(time(0)));

    try {
        Game game;
        game.run();
    }
    catch (const std::exception& e) {
        sf::RenderWindow errorWindow(sf::VideoMode(400, 200), "Error");
        sf::Font font;
        if (font.loadFromFile("resources/main_font.ttf")) {
            sf::Text errorText("Error: " + std::string(e.what()), font, 20);
            errorText.setFillColor(sf::Color::White);
            errorText.setPosition(20, 80);

            while (errorWindow.isOpen()) {
                sf::Event event;
                while (errorWindow.pollEvent(event)) {
                    if (event.type == sf::Event::Closed)
                        errorWindow.close();
                }

                errorWindow.clear(sf::Color::Red);
                errorWindow.draw(errorText);
                errorWindow.display();


            }
        }
        return 1;
    }

    return 0;
}