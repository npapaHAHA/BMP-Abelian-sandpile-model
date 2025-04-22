 #include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>
#include <limits> // Для INT64_MIN и INT64_MAX

// Структура для хранения аргументов командной строки
struct CommandLineArguments {
    std::string output_dir;
    uint64_t max_iter;
    uint64_t freq;
};

struct BMPFileHeader {
    uint16_t bfType = 0x4D42;        // "BM" идентификатор для формата BMP
    uint32_t bfSize;                 // Размер файла в байтах
    uint16_t bfReserved1 = 0;        // Зарезервировано, должно быть 0
    uint16_t bfReserved2 = 0;        // Зарезервировано, должно быть 0
    uint32_t bfOffBits;              // Смещение к данным пикселей
};

struct BMPInfoHeader {
    uint32_t biSize = 40;            // Размер этой структуры (40 байт)
    int32_t biWidth;                 // Ширина изображения в пикселях
    int32_t biHeight;                // Высота изображения в пикселях
    uint16_t biPlanes = 1;           // Число цветных плоскостей, всегда 1
    uint16_t biBitCount = 4;         // Количество бит на пиксель (4 бита для 16 цветов в палитре)
    uint32_t biCompression = 0;      // Метод сжатия, 0 для несжатых изображений
    uint32_t biSizeImage;            // Размер данных изображения (в байтах)
    int32_t biXPelsPerMeter = 0;     // Горизонтальное разрешение (необязательно)
    int32_t biYPelsPerMeter = 0;     // Вертикальное разрешение (необязательно)
    uint32_t biClrUsed = 5;          // Количество цветов в палитре
    uint32_t biClrImportant = 0;     // Количество важнейших цветов в палитре
};

// Функция для отображения помощи по использованию программы
void print_usage() {
    std::cout << "Use: SandpileModel -o <output_dir> -m <max_iter> -f <freq>\n";
}

// Функция для парсинга аргументов командной строки
CommandLineArguments parse_arguments(int argc, char* argv[]) {
    CommandLineArguments args;
    args.max_iter = 0;
    args.freq = 0;

    if (argc < 7) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if ((arg == "-m" || arg == "--max-iter") && i + 1 < argc) {
            args.max_iter = std::stoull(argv[++i]);
        } else if ((arg == "-f" || arg == "--freq") && i + 1 < argc) {
            args.freq = std::stoull(argv[++i]);
        } else {
            print_usage();
            exit(EXIT_FAILURE);
        }
    }

    return args;
}

// Функции для работы с файловой системой
bool directory_exists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

bool create_directory(const std::string& path) {
    if (!directory_exists(path)) {
        return std::filesystem::create_directories(path);
    }
    return true;
}

// Структура для хранения информации о ячейке
struct CellData {
    int16_t x;
    int16_t y;
    uint64_t grains;
};

// Функция для чтения входного TSV-файла и инициализации начального состояния
void read_input_file(const std::string& path, std::vector<CellData>& cells) {
    std::ifstream infile(path);
    if (!infile.is_open()) {
        std::cerr << "No open file: " << path << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(infile, line)) {
        CellData cell;
        std::istringstream iss(line);
        std::string token;
        int field = 0;
        while (std::getline(iss, token, '\t')) {
            switch (field) {
                case 0:
                    cell.x = static_cast<int16_t>(std::stoi(token));
                    break;
                case 1:
                    cell.y = static_cast<int16_t>(std::stoi(token));
                    break;
                case 2:
                    cell.grains = std::stoull(token);
                    break;
                default:
                    break;
            }
            ++field;
        }
        cells.push_back(cell);
    }

    infile.close();
}

// Класс для моделирования песчаной кучи
class SandpileModel {
public:
    SandpileModel() {
        iteration = 0;
        grid = nullptr;
        temp_grid = nullptr;
        min_x = min_y = max_x = max_y = 0;
        width = height = 0;
    }

    ~SandpileModel() {
        free_grids();
    }

    void initialize(const std::vector<CellData>& cells) {
        min_x = min_y = std::numeric_limits<int64_t>::max();
        max_x = max_y = std::numeric_limits<int64_t>::min();

        for (const auto& cell : cells) {
            if (cell.x < min_x) min_x = cell.x;
            if (cell.x > max_x) max_x = cell.x;
            if (cell.y < min_y) min_y = cell.y;
            if (cell.y > max_y) max_y = cell.y;
        }

        width = max_x - min_x + 1;
        height = max_y - min_y + 1;

        allocate_grids();

        for (const auto& cell : cells) {
            int64_t x = cell.x - min_x;
            int64_t y = cell.y - min_y;
            grid[y][x] = cell.grains;
        }
    }

    bool iterate() {
        bool changed = false;
        for (size_t y = 0; y < height; ++y) {
            std::memcpy(temp_grid[y], grid[y], width * sizeof(uint64_t));
        }

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                uint64_t grains = temp_grid[y][x];
                if (grains >= 4) {
                    changed = true;
                    uint64_t overflow = grains / 4;
                    grid[y][x] -= overflow * 4;

                    if (y > 0) {
                        grid[y - 1][x] += overflow;
                    } else {
                        expand_grid_up();
                        grid[0][x] += overflow;
                        y++;
                    }
                    if (y < height - 1) {
                        grid[y + 1][x] += overflow;
                    } else {
                        expand_grid_down();
                        grid[height - 1][x] += overflow;
                    }
                    if (x > 0) {
                        grid[y][x - 1] += overflow;
                    } else {
                        expand_grid_left();
                        grid[y][0] += overflow;
                        x++;
                    }
                    if (x < width - 1) {
                        grid[y][x + 1] += overflow;
                    } else {
                        expand_grid_right();
                        grid[y][width - 1] += overflow;
                    }
                }
            }
        }

        iteration++;
        return changed;
    }

    void save_state_to_bmp(const std::string& filename) {
        write_bmp(filename);
    }

    uint64_t get_iteration() const {
        return iteration;
    }

private:
    uint64_t iteration;
    uint64_t** grid;
    uint64_t** temp_grid;
    int64_t min_x, max_x;
    int64_t min_y, max_y;
    size_t width, height;

    void allocate_grids() {
        grid = new uint64_t*[height];
        temp_grid = new uint64_t*[height];
        for (size_t y = 0; y < height; ++y) {
            grid[y] = new uint64_t[width];
            temp_grid[y] = new uint64_t[width];
            std::memset(grid[y], 0, width * sizeof(uint64_t));
            std::memset(temp_grid[y], 0, width * sizeof(uint64_t));
        }
    }

    void free_grids() {
        if (grid) {
            for (size_t y = 0; y < height; ++y) {
                delete[] grid[y];
                delete[] temp_grid[y];
            }
            delete[] grid;
            delete[] temp_grid;
        }
    }

    void expand_grid_up() {
        size_t new_height = height + 1;
        uint64_t** new_grid = new uint64_t*[new_height];
        uint64_t** new_temp_grid = new uint64_t*[new_height];
        new_grid[0] = new uint64_t[width];
        new_temp_grid[0] = new uint64_t[width];
        std::memset(new_grid[0], 0, width * sizeof(uint64_t));
        std::memset(new_temp_grid[0], 0, width * sizeof(uint64_t));
        for (size_t y = 0; y < height; ++y) {
            new_grid[y + 1] = grid[y];
            new_temp_grid[y + 1] = temp_grid[y];
        }
        delete[] grid;
        delete[] temp_grid;
        grid = new_grid;
        temp_grid = new_temp_grid;
        height = new_height;
        min_y--;
    }

    void expand_grid_down() {
        size_t new_height = height + 1;
        uint64_t** new_grid = new uint64_t*[new_height];
        uint64_t** new_temp_grid = new uint64_t*[new_height];
        for (size_t y = 0; y < height; ++y) {
            new_grid[y] = grid[y];
            new_temp_grid[y] = temp_grid[y];
        }
        new_grid[height] = new uint64_t[width];
        new_temp_grid[height] = new uint64_t[width];
        std::memset(new_grid[height], 0, width * sizeof(uint64_t));
        std::memset(new_temp_grid[height], 0, width * sizeof(uint64_t));
        delete[] grid;
        delete[] temp_grid;
        grid = new_grid;
        temp_grid = new_temp_grid;
        height = new_height;
        max_y++;
    }

    void expand_grid_left() {
        size_t new_width = width + 1;
        for (size_t y = 0; y < height; ++y) {
            uint64_t* new_row = new uint64_t[new_width];
            uint64_t* new_temp_row = new uint64_t[new_width];
            std::memset(new_row, 0, new_width * sizeof(uint64_t));
            std::memset(new_temp_row, 0, new_width * sizeof(uint64_t));
            std::memcpy(new_row + 1, grid[y], width * sizeof(uint64_t));
            std::memcpy(new_temp_row + 1, temp_grid[y], width * sizeof(uint64_t));
            delete[] grid[y];
            delete[] temp_grid[y];
            grid[y] = new_row;
            temp_grid[y] = new_temp_row;
        }
        width = new_width;
        min_x--;
    }

    void expand_grid_right() {
        size_t new_width = width + 1;
        for (size_t y = 0; y < height; ++y) {
            uint64_t* new_row = new uint64_t[new_width];
            uint64_t* new_temp_row = new uint64_t[new_width];
            std::memset(new_row, 0, new_width * sizeof(uint64_t));
            std::memset(new_temp_row, 0, new_width * sizeof(uint64_t));
            std::memcpy(new_row, grid[y], width * sizeof(uint64_t));
            std::memcpy(new_temp_row, temp_grid[y], width * sizeof(uint64_t));
            delete[] grid[y];
            delete[] temp_grid[y];
            grid[y] = new_row;
            temp_grid[y] = new_temp_row;
        }
        width = new_width;
        max_x++;
    }

    void write_bmp(const std::string& filename) {
        const uint16_t bits_per_pixel = 4;
        const uint32_t row_size = ((width * bits_per_pixel + 31) / 32) * 4;
        const uint32_t pixel_array_size = row_size * height;
        const uint32_t file_header_size = 14;
        const uint32_t dib_header_size = 40;
        const uint32_t color_table_size = 16 * 4;
        const uint32_t pixel_data_offset = file_header_size + dib_header_size + color_table_size;
        const uint32_t file_size = pixel_data_offset + pixel_array_size;
        uint8_t* pixel_data = new uint8_t[pixel_array_size];
        std::memset(pixel_data, 0x00, pixel_array_size);

        for (size_t y = 0; y < height; ++y) {
            uint8_t* row = pixel_data + (height - 1 - y) * row_size;
            size_t bit_index = 0;
            for (size_t x = 0; x < width; ++x) {
                uint64_t grains = grid[y][x];
                uint8_t color_index;
                if (grains == 0)
                    color_index = 0;
                else if (grains == 1)
                    color_index = 1;
                else if (grains == 2)
                    color_index = 2;
                else if (grains == 3)
                    color_index = 3;
                else
                    color_index = 4;
                if (bit_index % 2 == 0) {
                    row[bit_index / 2] |= (color_index << 4);
                } else {
                    row[bit_index / 2] |= (color_index & 0x0F);
                }
                ++bit_index;
            }
        }

        std::ofstream outfile(filename, std::ios::binary);
        if (!outfile.is_open()) {
            std::cerr << "Не удалось открыть файл для записи: " << filename << std::endl;
            delete[] pixel_data;
            return;
        }

        uint8_t file_header[14] = {
            'B', 'M',
            0, 0, 0, 0,
            0, 0,
            0, 0,
            0, 0, 0, 0
        };
        *reinterpret_cast<uint32_t*>(&file_header[2]) = file_size;
        *reinterpret_cast<uint32_t*>(&file_header[10]) = pixel_data_offset;
        outfile.write(reinterpret_cast<char*>(file_header), sizeof(file_header));

        uint8_t dib_header[40] = {0};
        *reinterpret_cast<uint32_t*>(&dib_header[0]) = dib_header_size;
        *reinterpret_cast<int32_t*>(&dib_header[4]) = static_cast<int32_t>(width);
        *reinterpret_cast<int32_t*>(&dib_header[8]) = static_cast<int32_t>(height);
        *reinterpret_cast<uint16_t*>(&dib_header[12]) = 1;
        *reinterpret_cast<uint16_t*>(&dib_header[14]) = bits_per_pixel;
        *reinterpret_cast<uint32_t*>(&dib_header[16]) = 0;
        *reinterpret_cast<uint32_t*>(&dib_header[20]) = pixel_array_size;
        *reinterpret_cast<int32_t*>(&dib_header[24]) = 0;
        *reinterpret_cast<int32_t*>(&dib_header[28]) = 0;
        *reinterpret_cast<uint32_t*>(&dib_header[32]) = 16;
        *reinterpret_cast<uint32_t*>(&dib_header[36]) = 0;
        outfile.write(reinterpret_cast<char*>(dib_header), sizeof(dib_header));

        uint8_t color_table[64] = {
            255, 255, 255, 0,
            0, 255, 0, 0,
            255, 0, 255, 0,
            0, 255, 255, 0,
            0, 0, 0, 0
        };
        std::memset(color_table + 20, 0, 64 - 20);
        outfile.write(reinterpret_cast<char*>(color_table), sizeof(color_table));

        outfile.write(reinterpret_cast<char*>(pixel_data), pixel_array_size);

        outfile.close();
        delete[] pixel_data;
    }
};

// Главная функция программы
int main(int argc, char* argv[]) {
    CommandLineArguments args = parse_arguments(argc, argv);

    std::string input_file = "input.tsv";
    if (!create_directory(args.output_dir)) {
        std::cerr << "No new directory: " << args.output_dir << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<CellData> cells;
    read_input_file(input_file, cells);

    SandpileModel model;
    model.initialize(cells);

    bool changed = true;
    uint64_t max_iter = args.max_iter;
    uint64_t freq = args.freq;
    uint64_t iteration = 0;

    while (changed && iteration < max_iter) {
        changed = model.iterate();
        iteration = model.get_iteration();
        std::cout << "Текущая итерация: " << iteration << std::endl;

        if (freq != 0 && iteration % freq == 0) {
            std::string filename = args.output_dir + "/state_" + std::to_string(iteration) + ".bmp";
            model.save_state_to_bmp(filename);
            std::cout << "Сохранено изображение: " << filename << std::endl;
        }
    }

    if (freq == 0 || iteration % freq != 0) {
        std::string filename = args.output_dir + "/state_" + std::to_string(iteration) + ".bmp";
        model.save_state_to_bmp(filename);
        std::cout << "Сохранено последнее изображение: " << filename << std::endl;
    }

    std::cout << "Моделирование завершено на итерации: " << iteration << std::endl;
    return EXIT_SUCCESS;
}