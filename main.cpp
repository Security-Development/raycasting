#include <iostream>
#include <vector>
#include <cmath>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define MAX_WIDTH 64
#define MAX_HIEGH 64
#define DEG2RAD(deg) ((deg) * M_PI / 180.0f)
#define DEFAULT_FOV DEG2RAD(60)

struct Vector3 {
    float x, y, z;
    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct Player {
    Vector3 position;
    float yaw, pitch;
    float fov;
    int numRays;
};

int worldMap[MAX_HIEGH][MAX_WIDTH];

void initMap() {
    for (int z = 0; z < MAX_HIEGH; ++z) {
        for (int x = 0; x < MAX_WIDTH; ++x) {
            // 외벽
            if (z == 0 || z == MAX_HIEGH - 1 || x == 0 || x == MAX_WIDTH - 1)
                worldMap[z][x] = 1;
            else
                worldMap[z][x] = 0;
        }
    }

    for (int x = 2; x < 60; ++x) worldMap[3][x] = 1;
    for (int x = 4; x < 62; ++x) worldMap[6][x] = 1;
    for (int x = 1; x < 58; ++x) worldMap[9][x] = 1;
    for (int x = 5; x < 60; x += 4) worldMap[12][x] = 1;

    for (int z = 4; z < 30; ++z) worldMap[z][10] = 1;
    for (int z = 2; z < 25; ++z) worldMap[z][20] = 1;
    for (int z = 6; z < 28; ++z) worldMap[z][35] = 1;
    for (int z = 1; z < 20; ++z) worldMap[z][50] = 1;

    for (int x = 15; x < 25; ++x) worldMap[15][x] = 1;
    for (int x = 40; x < 55; ++x) worldMap[18][x] = 1;
    for (int x = 25; x < 35; ++x) worldMap[22][x] = 1;

    worldMap[3][5] = 0;
    worldMap[6][10] = 0;
    worldMap[9][15] = 0;
    worldMap[15][20] = 0;
    worldMap[18][42] = 0;
    worldMap[22][30] = 0;
}

bool isWall(float x, float z) {
    int ix = (int)floor(x);
    int iz = (int)floor(z);
    return ix < 0 || ix >= MAX_WIDTH || iz < 0 || iz >= MAX_HIEGH || worldMap[iz][ix];
}

int kbhit() {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt; newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}

char getch() {
    return getchar();
}

void getTerminalSize(int& width, int& height) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    width = w.ws_col;
    height = w.ws_row;
}

void RenderFrame(Player& character) {
    int termWidth, termHeight;
    getTerminalSize(termWidth, termHeight);

    int miniSize = std::min(termWidth / 2, termHeight / 3);
    if (miniSize < 9) miniSize = 9;
    if (miniSize % 2 == 0) miniSize++;

    int screenHeight = termHeight - miniSize - 3;
    character.numRays = termWidth;

    std::vector<std::string> screen(screenHeight, std::string(termWidth, ' '));
    std::vector<std::string> mini(miniSize, std::string(miniSize * 2, ' '));

    for (int i = 0; i < character.numRays; ++i) {
        float angle = character.yaw - character.fov / 2 + (float)i / character.numRays * character.fov;
        float dx = cos(angle), dz = sin(angle), dist = 0;
        while (!isWall(character.position.x + dx * dist, character.position.z + dz * dist) && dist < MAX_WIDTH)
            dist += 0.1f;

        float corrected = dist * cos(angle - character.yaw);
        int wallHeight = (int)(screenHeight / (corrected + 0.01f));
        int ceiling = (screenHeight - wallHeight) / 2;
        int floor = screenHeight - ceiling;
        char shade = ' ';
        if (corrected < 1.0f) shade = '#';
        else if (corrected < 2.0f) shade = '@';
        else if (corrected < 3.0f) shade = 'O';
        else if (corrected < 4.0f) shade = '+';
        else if (corrected < 6.0f) shade = ':';
        else if (corrected < 8.0f) shade = '.';

        for (int y = 0; y < screenHeight; ++y)
            screen[y][i] = (y < ceiling) ? ' ' : (y < floor ? shade : '-');
    }

    // ---- 미니맵 렌더링 ----
    int centerX = (int)(character.position.x);
    int centerZ = (int)(character.position.z);
    int half = miniSize / 2;

    for (int dz = -half; dz <= half; ++dz) {
        for (int dx = -half; dx <= half; ++dx) {
            int wx = centerX + dx;
            int wz = centerZ + dz;

            int mx = dx + half;
            int mz = dz + half;

            if (mx * 2 + 1 >= mini[0].size() || mz >= mini.size())
                continue;

            char ch = '.';
            if (wx < 0 || wx >= MAX_WIDTH || wz < 0 || wz >= MAX_HIEGH)
                ch = ' ';
            else if (worldMap[wz][wx])
                ch = '#';

            mini[mz][mx * 2] = ch;
            mini[mz][mx * 2 + 1] = ' ';
        }
    }

    mini[half][half * 2] = 'P';

    float rayDX = cos(character.yaw);
    float rayDZ = sin(character.yaw);
    for (int step = 1; step <= 8; ++step) {
        float fx = character.position.x + rayDX * step;
        float fz = character.position.z + rayDZ * step;

        if (isWall(fx, fz)) break; 

        int dx = (int)(fx - centerX);
        int dz = (int)(fz - centerZ);
        int mx = dx + half;
        int mz = dz + half;
        if (mx * 2 + 1 >= mini[0].size() || mz >= mini.size()) break;
        if (mini[mz][mx * 2] == 'P') continue;
        mini[mz][mx * 2] = '*';
    }

    std::cout << "\033[2J\033[H";
    for (int z = 0; z < miniSize; ++z) std::cout << mini[z] << '\n';
    for (int y = 0; y < screenHeight; ++y) std::cout << screen[y] << '\n';
}

int main() {
    initMap();
    Player character = { Vector3(14, 0, 14), DEG2RAD(90), 0, DEFAULT_FOV, 80 };
    const float moveSpeed = 0.2f, rotateSpeed = DEG2RAD(5);

    while (true) {
        RenderFrame(character);
        std::cout << "pos=(" << character.position.x << ", " << character.position.z
                  << ") yaw=" << character.yaw * 180 / M_PI << " deg\n";
        std::cout << "[WASD]: Move / Q: Quit\n";

        if (kbhit()) {
            char key = getch();
            float dx = cos(character.yaw), dz = sin(character.yaw);
            if (key == 'w' || key == 'W') {
                float nx = character.position.x + dx * moveSpeed;
                float nz = character.position.z + dz * moveSpeed;
                if (!isWall(nx, nz)) character.position.x = nx, character.position.z = nz;
            }
            else if (key == 's' || key == 'S') {
                float nx = character.position.x - dx * moveSpeed;
                float nz = character.position.z - dz * moveSpeed;
                if (!isWall(nx, nz)) character.position.x = nx, character.position.z = nz;
            }
            else if (key == 'a' || key == 'A') character.yaw -= rotateSpeed;
            else if (key == 'd' || key == 'D') character.yaw += rotateSpeed;
            else if (key == 'q' || key == 'Q') break;
        }

        usleep(33000);
    }

    return 0;
}
