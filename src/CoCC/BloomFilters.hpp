/*
 * BloomFilters.hpp
 *
 *  Created on: Aug 17, 2020
 *      Author: markus
 */

#ifndef COCC_BLOOMFILTERS_HPP_
#define COCC_BLOOMFILTERS_HPP_

#include <functional>
#include <vector>

template<typename storageType, typename keyType, typename hashType = std::hash<keyType>>
class AbstractBloomFilter {

public:
    AbstractBloomFilter(const size_t expectedElements, const double falseErrorRate)
            : numHashes((uint) (log(falseErrorRate) / log(0.6185) * log(2))),       // optimum number of hash functions ~ m/n * ln(2)
              storage(expectedElements * log(falseErrorRate) / log(0.6185))         // optimum bits per element m/n ~ log_0.6185(falseErrorRate)
    {

    }

    virtual ~AbstractBloomFilter() { };

    void add(const keyType& key) {
        size_t hash[2];

        computeHash(key, hash);

        for (uint n = 0; n < numHashes; n++) {
            const size_t pos = nthHash(n, hash);

            setAt(pos);
        }

        count++;
    }

    bool contains(const keyType& key) {
        bool result = true;
        size_t hash[2];

        computeHash(key, hash);

        for (uint n = 0; n < numHashes; n++) {
            const size_t pos = nthHash(n, hash);

            result &= checkAt(pos);
        }

        return result;
    }

    virtual void reset() {
        count = 0;
    }

    size_t size() {
        return count;
    }

    virtual size_t hashValue() = 0;

    std::string str() {
        std::stringstream result;

        for (size_t i = 0; i < storage.size(); i++) {
            if (storage[i] != 0) {
                result <<  "[" << i << "]=" << (size_t) storage[i] << "; ";
            }
        }

        return result.str();
    }

protected:

    uint numHashes;
    size_t count = 0;
    std::vector<storageType> storage;

    AbstractBloomFilter(const size_t size, const uint numHashes)
            : numHashes(numHashes),
              storage(size) {
    }

    virtual void setAt(const size_t index) = 0;
    virtual bool checkAt(const size_t index) = 0;

    size_t baseHash(const size_t key) {
        // djb2 hash function
        size_t hash = 5381;
        const uint8_t * ptr = reinterpret_cast<const uint8_t *>(&key);

        for (size_t i = 0; i < sizeof(key); i++) {
            hash = hash * 33 + *(ptr++);
        }

        return hash;
    }

    void computeHash(const keyType& key, size_t (&hashes)[2]) {
        const size_t hashedKey = hashType{}(key);

        // rather crude method to derive two hash functions
        // 2^17 - 1 and 2^19-1 are both primes
        hashes[0] = baseHash(hashedKey * ((2 << 17) - 1));
        hashes[1] = baseHash(hashedKey * ((2 << 19) - 1));
    }

    size_t nthHash(const uint n, size_t (&hashes)[2]) {
        return (hashes[0] + n * hashes[1]) % storage.size();
    }
};


template<typename keyType, typename hashType = std::hash<keyType>>
class SimpleBloomFilter : public AbstractBloomFilter<bool, keyType, hashType> {
protected:
    typedef AbstractBloomFilter<bool, keyType, hashType> base;

public:
    SimpleBloomFilter(const size_t expectedElements, const double falseErrorRate)
            : base::AbstractBloomFilter(expectedElements, falseErrorRate) {
    }

    virtual void reset() override {
        base::reset();

        for (size_t i = 0; i < this->storage.size(); i++) {
            this->storage[index] = false;
        }
    }

    virtual size_t hashValue() override {
        const uint blockSize = sizeof(size_t);
        const uint blocks = (this->storage.size() + blockSize - 1) / blockSize; // computes ceil() for division
        size_t result = 0;
        size_t currentBlock = 0;

        for (uint block = 0; block < blocks; block++) {
            for (uint bit = 0; bit < blockSize; bit++) {
                currentBlock = currentBlock << 1 | this->storage[block * blockSize + bit];
            }

            if (block != 0) {
                result = result * 31 + currentBlock;
            } else {
                result = currentBlock;
            }
        }

        return result;
    }

protected:
    virtual void setAt(const size_t index) override {
        this->storage[index] = true;
    }

    virtual bool checkAt(const size_t index) override {
        return this->storage[index];
    }
};


template<typename keyType, typename hashType = std::hash<keyType>>
class CountingBloomFilter : public AbstractBloomFilter<uint8_t, keyType, hashType> {
protected:
    typedef AbstractBloomFilter<uint8_t, keyType, hashType> base;

public:

    CountingBloomFilter(const size_t expectedElements, const double falseErrorRate)
            : base::AbstractBloomFilter(expectedElements, falseErrorRate) {
    }

    virtual void reset() override {
        base::reset();

        for (size_t i = 0; i < this->storage.size(); i++) {
            this->storage[i] = 0;
        }
    }

    virtual size_t hashValue() override {
        size_t result = 0;

        for (size_t i = 0; i < this->storage.size(); i++) {
            if (i != 0) {
                result = 31 * result + this->storage[i];
            } else {
                result = this->storage[i];
            }
        }

        return result;
    }

    void remove(const keyType& key) {
        size_t hash[2];

        this->computeHash(key, hash);

        for (uint n = 0; n < this->numHashes; n++) {
            const size_t pos = this->nthHash(n, hash);

            if (this->storage[pos] > 0) {
                this->storage[pos]--;
            }
        }

        this->count--;
    }

protected:
    virtual void setAt(const size_t index) override {
        this->storage[index]++;
    }

    virtual bool checkAt(const size_t index) override {
        return this->storage[index] > 0;
    }
};

#endif /* COCC_BLOOMFILTERS_HPP_ */
