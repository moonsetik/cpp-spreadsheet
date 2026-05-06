#pragma once

#include "cell.h"
#include "common.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct PositionHasher {
    size_t operator()(const Position& pos) const noexcept {
        return std::hash<int>()(pos.row) ^ (std::hash<int>()(pos.col) << 1);
    }
};

class Sheet : public SheetInterface {
public:
    ~Sheet() override;

    void SetCell(Position pos, std::string text) override;
    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;
    void ClearCell(Position pos) override;
    Size GetPrintableSize() const override;
    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    void InvalidateCell(Position pos);
    bool HasCycleAfterAdding(Position target, const std::vector<Position>& refs) const;

private:
    void InvalidateCellDFS(Position pos, std::unordered_set<Position, PositionHasher>& visited);

    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
};