#include <iostream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <random>
#include <climits>

template <typename T>
class CuckooHashTable {
    static const int MAX_LOOP = 1000;
    std::vector<std::optional<T>> table1, table2;
    int size;
    int count1, count2; // Counters for the number of elements in each table

    int hash1(T key) {
        const int prime = 307;
        return (key + prime) % size;
    }

    int hash2(T key) {
        const int prime = 709;
        return (prime * key + 1) % size;
    }

public:
    CuckooHashTable(int size) : size(size), count1(0), count2(0) {
        table1.resize(size);
        table2.resize(size);
    }

    void insert(T key) {
        for (int i = 0, table = 0; i < MAX_LOOP; ++i, table = !table) {
            if (table == 0) {
                int index = hash1(key) % size;
                if (!table1[index]) {
                    table1[index] = key;
                    count1++;
                    return;
                }
                std::swap(key, *table1[index]);
            } else {
                int index = hash2(key) % size;
                if (!table2[index]) {
                    table2[index] = key;
                    count2++;
                    return;
                }
                std::swap(key, *table2[index]);
            }
        }
        throw std::runtime_error("Max loop count reached, rehashing needed");
    }

    int getTotalCount() const {
        return count1 + count2;
    }

    double getLoadFactor(int tableIndex) {
        switch (tableIndex) {
            case 1: return static_cast<double>(count1) / size;
            case 2: return static_cast<double>(count2) / size;
            default: throw std::invalid_argument("Invalid table index");
        }
    }

    void printTables() const {
        std::cout << "Table 1:" << std::endl;
        for (int i = 0; i < size; ++i) {
            if (table1[i]) {
                std::cout << "Index " << i << ": " << *table1[i] << std::endl;
            }
        }

        std::cout << "Table 2:" << std::endl;
        for (int i = 0; i < size; ++i) {
            if (table2[i]) {
                std::cout << "Index " << i << ": " << *table2[i] << std::endl;
            }
        }
    }
};

int main() {
    const int TABLE_SIZE = 1000;
    CuckooHashTable<int> hashTable(TABLE_SIZE);
    
    try {
        for (int i = 1; i <= 100000; ++i) {
            hashTable.insert(i*100+7+rand()%100);
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    hashTable.printTables();
    std::cout << "Load Factor Table 1: " << hashTable.getLoadFactor(1) << std::endl;
    std::cout << "Load Factor Table 2: " << hashTable.getLoadFactor(2) << std::endl;

    return 0;
}
