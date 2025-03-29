#include <stdint.h>
#include <vector>

class Sequence {
private:
    std::vector<uint8_t> values;
    size_t index;

public:
    // Konstruktor
    Sequence(std::vector<uint8_t> vals) : values(std::move(vals)), index(0) {}

    // Cast-Operator to uint8_t
    operator uint8_t() const {
        return values[index];
    }

    // Prefix-Inkrementoperator (++seq)
    Sequence& operator++() {
        index = (index + 1) % values.size(); // Zyklisch weiterzählen
        return *this;
    }

    inline std::size_t length() {
        return values.size();
    }

    inline void reset() {
        index = 0;
    }
};
