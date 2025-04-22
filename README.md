# Lab Work 3: Abelian Sandpile Model + BMP Output

A simplified version of the [Abelian sandpile model](https://en.wikipedia.org/wiki/Abelian_sandpile_model), implemented as a console C++ application that saves visual states of the simulation as BMP images.

---

## 📌 Task Overview

The program reads an initial configuration from a TSV file and simulates the sandpile model step by step, outputting BMP images based on the state of the grid.

### 🔧 Features

- Dynamically resizing 2D grid
- Manual BMP image generation (no external libraries)
- Saves intermediate states as `.bmp`
- Command-line interface

---

## 💡 Command-Line Arguments

-i, --input Path to a TSV file with initial state -o, --output Directory where output BMPs will be saved -m, --max-iter Maximum number of iterations -f, --freq Frequency of BMP saving (0 = only final state)


---

## 📁 Input Format

Initial state is stored in a **TSV (tab-separated)** file.  
Each line represents a single cell:

x<TAB>y<TAB>number_of_grains


- `x`, `y` — integer coordinates (`int16_t`)
- `number_of_grains` — non-negative integer (`uint64_t`)

The program determines the **minimal bounding rectangle** that fits all initial cells.

---

## 🔄 Simulation Rules

- Each cell can hold grains.
- If grains > 3, they "topple" and distribute 1 grain to each of the four neighbors (up/down/left/right).
- Grid expands if needed.
- Simulation ends if:
  - A stable state is reached
  - Maximum iteration count is hit

---

## 🖼️ BMP Output Format

- BMP image size = current grid size
- Each pixel represents a single cell
- Pixel color depends on grain count:
  - `0` grains → white
  - `1` → green
  - `2` → purple
  - `3` → yellow
  - `>3` → black
- Each pixel must take ≤ 4 bits

> You must implement BMP saving manually — no libraries allowed!

---

## ⚠️ Constraints

- No external libraries (only STL headers allowed)
- No `std::vector`, `std::list`, etc.
- Use CMake for build system
- Implement BMP saving logic from scratch
- Consider **memory alignment** (use proper struct packing with `#pragma pack`)

---

## 🛠️ Tools / Suggestions

- Use `std::filesystem` for file path handling
- Handle pixel alignment and padding manually in BMP
- You may define your own project structure (but must use CMake)

---

## 🧪 Example BMP Output Mapping

| Grain Count | Pixel Color |
|-------------|-------------|
| 0           | White       |
| 1           | Green       |
| 2           | Purple      |
| 3           | Yellow      |
| >3          | Black       |

---

## 🧭 Sample Run

```bash
./sandpile -i input.tsv -o ./frames -m 10000 -f 100
Loads input.tsv

Simulates up to 10,000 steps

Saves a BMP every 100 steps into ./frames/
