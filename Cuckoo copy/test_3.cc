#include <iostream>
#include <vector>
#include <optional>
#include <tuple>
#include <stdexcept>
#include <random>
#include <climits>
#include <limits>
#include <algorithm>
#include <set>
#include "xxhash.h"

template <typename T>
class CuckooHashTable {
    static const int MAX_LOOP = 50;
    std::vector<std::optional<T>> table1, table2, table3;
    int size;
    int count1, count2, count3; // Counters for the number of elements in each table

    int hash1(T key) {
        const int prime = 709; // Choose a different prime number
        return ((key * prime) ^ (key >> 4)) % size;
    }

   int hash2(T key) {
        const int prime1 = 307; // Another prime number
        const int prime2 = 98317; // Yet another prime number
        return ((key * prime1) + prime2) ^ (key >> 3) % size;
    }

    int hash3(T key) {
        // Using a composition that differs significantly from hash1 and hash2
        return XXH32(&key, sizeof(key), 0) % size;
    }

    std::optional<T>& getTable(int tableIndex, int index) {
        switch (tableIndex) {
            case 1: return table1[index];
            case 2: return table2[index];
            case 3: return table3[index];
            default: throw std::invalid_argument("Invalid table index");
        }
    }

public:
    CuckooHashTable(int size) : size(size), count1(0), count2(0), count3(0) {
        table1.resize(size);
        table2.resize(size);
        table3.resize(size);
    }

   void insert(T key, std::tuple<int, int, int> priority) {
    int loop_count = 0;
    int initial_key = key;
    int initial_table = std::get<0>(priority);
    while (loop_count < MAX_LOOP) {
        for (int i = 0; i < 3; ++i) {
            int tableIndex;
            switch (i) {
                case 0:
                    tableIndex = std::get<0>(priority);
                    break;
                case 1:
                    tableIndex = std::get<1>(priority);
                    break;
                case 2:
                    tableIndex = std::get<2>(priority);
                    break;
            }

            int index;
            switch (tableIndex) {
                case 1:
                    index = hash1(key) % size;
                    if (!table1[index]) {
                        table1[index] = key;
                        count1++;

                        std::cout<<"placed "<<key<<" in table 1"<<" at index "<<index<<std::endl;
                       // printTables();
                        return;
                    }
                    break;
                case 2:
                    index = hash2(key) % size;
                    if (!table2[index]) {
                        table2[index] = key;
                        count2++;
                        std::cout<<"placed "<<key<<" in table 2"<<" at index "<<index<<std::endl;
                        //printTables();
                        return;
                    }
                    break;
                case 3:
                    index = hash3(key) % size;
                    if (!table3[index]) {
                        table3[index] = key;
                        count3++;
                        std::cout<<"placed "<<key<<" in table 3"<<" at index "<<index<<std::endl;
                        //printTables();
                        return;
                    }
                    break;
                default:
                    throw std::invalid_argument("Invalid table index in priority tuple");
            }
            std::cout<<"i = "<<i<<" index = "<<index<<" tableIndex = "<<tableIndex<<std::endl;
            std::cout<<"Swapping "<<key<<" with "<<*getTable(tableIndex, index)<<std::endl;
            std::swap(key, *getTable(tableIndex, index));

            if(key==initial_key && tableIndex==initial_table)
            {
                std::cout<<"a circle has been detected"<<std::endl;
                throw std::runtime_error("A circle is finded, rehashing needed");

            }


            //printTables();
        }
    

        loop_count++;
    }
    throw std::runtime_error("Max loop count reached, rehashing needed");
}


int getTotalCount() const {
    return count1 + count2 + count3;
}


double getLoadFactor(int tableIndex) {
    switch (tableIndex) {
        case 1: return static_cast<double>(count1) / size;
        case 2: return static_cast<double>(count2) / size;
        case 3: return static_cast<double>(count3) / size;
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
    std::cout << std::endl;
    std::cout << "Table 2:" << std::endl;
    for (int i = 0; i < size; ++i) {
        if (table2[i]) {
            std::cout << "Index " << i << ": " << *table2[i] << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << "Table 3:" << std::endl;
    for (int i = 0; i < size; ++i) {
        if (table3[i]) {
            std::cout << "Index " << i << ": " << *table3[i] << std::endl;
        }
    }
    std::cout << std::endl;
}




    // Add more functions (like delete, get) as needed
};

int main() {
    const int TABLE_SIZE = 1000;
    CuckooHashTable<int> hashTable(TABLE_SIZE);
    
    std::random_device rd; // Seed for the random number generator
    std::mt19937 gen(rd()); // Standard mersenne_twister_engine
    //std::uniform_int_distribution<> dis(1, 3); // Distribution for table indices
    std::set<int> generatedKeys;


    try {
        for (int i = 1; i < INT_MAX; ++i) {


            //randomly generate a key
            std::uniform_int_distribution<> dis(1, 1000000);
            int key = dis(gen);

            // Check if the key has already been generated
            if (generatedKeys.find(key) != generatedKeys.end()) {
                continue;
            }

            generatedKeys.insert(key);


            // Generate a random priority tuple for each insertion
            std::array<int, 3> priorities = {1, 2, 3};
            std::shuffle(priorities.begin(), priorities.end(), gen); // Shuffle to get a random permutation
            auto priority = std::make_tuple(priorities[0], priorities[1], priorities[2]);
            std::cout << "Inserting " << key << " with priority (" << std::get<0>(priority) << ", " << std::get<1>(priority) << ", " << std::get<2>(priority) << ")" << std::endl;
            hashTable.insert(key, priority);
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Reached limit with " << hashTable.getTotalCount() << " elements." << std::endl;
    }


    std::cout << "Load Factor Table 1: " << hashTable.getLoadFactor(1) << std::endl;
    std::cout << "Load Factor Table 2: " << hashTable.getLoadFactor(2) << std::endl;
    std::cout << "Load Factor Table 3: " << hashTable.getLoadFactor(3) << std::endl;


    return 0;
}
