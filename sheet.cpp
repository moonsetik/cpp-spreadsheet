#include "sheet.h"
#include "formula.h"
#include <algorithm>
#include <functional>
#include <sstream>
#include <variant>

Sheet::~Sheet() = default;

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) throw InvalidPositionException("Invalid position");

    if (text.empty()) {
        ClearCell(pos);
        return;
    }

    auto& cell_ptr = cells_[pos];
    if (!cell_ptr) {
        cell_ptr = std::make_unique<Cell>(*this);
    }
    cell_ptr->SetPosition(pos);
    cell_ptr->Set(std::move(text));
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) throw InvalidPositionException("Invalid position");
    auto it = cells_.find(pos);
    return it != cells_.end() ? it->second.get() : nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    if (!pos.IsValid()) throw InvalidPositionException("Invalid position");
    auto it = cells_.find(pos);
    return it != cells_.end() ? it->second.get() : nullptr;
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) throw InvalidPositionException("Invalid position");
    auto it = cells_.find(pos);
    if (it != cells_.end()) {
        it->second->Clear();
        cells_.erase(it);
    }
}

Size Sheet::GetPrintableSize() const {
    int max_row = -1, max_col = -1;
    for (const auto& [pos, cell] : cells_) {
        if (cell) {
            max_row = std::max(max_row, pos.row);
            max_col = std::max(max_col, pos.col);
        }
    }
    return {max_row + 1, max_col + 1};
}

void Sheet::PrintValues(std::ostream& output) const {
    Size size = GetPrintableSize();
    for (int r = 0; r < size.rows; ++r) {
        for (int c = 0; c < size.cols; ++c) {
            if (c > 0) output << '\t';
            Position pos{r, c};
            auto it = cells_.find(pos);
            if (it != cells_.end() && it->second) {
                auto val = it->second->GetValue();
                std::visit([&output](const auto& v) { output << v; }, val);
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    Size size = GetPrintableSize();
    for (int r = 0; r < size.rows; ++r) {
        for (int c = 0; c < size.cols; ++c) {
            if (c > 0) output << '\t';
            Position pos{r, c};
            auto it = cells_.find(pos);
            if (it != cells_.end() && it->second) output << it->second->GetText();
        }
        output << '\n';
    }
}

void Sheet::InvalidateCell(Position pos) {
    std::unordered_set<Position, PositionHasher> visited;
    InvalidateCellDFS(pos, visited);
}

void Sheet::InvalidateCellDFS(Position pos, std::unordered_set<Position, PositionHasher>& visited) {
    if (visited.count(pos)) return;
    visited.insert(pos);

    auto it = cells_.find(pos);
    if (it != cells_.end() && it->second) {
        it->second->InvalidateCache();
        for (const auto& dep_pos : it->second->GetIncomingDependencies()) {
            InvalidateCellDFS(dep_pos, visited);
        }
    }
}

bool Sheet::HasCycleAfterAdding(Position target, const std::vector<Position>& refs) const {
    for (const auto& ref : refs) {
        std::unordered_set<Position, PositionHasher> visited;
        std::function<bool(Position)> dfs = [&](Position cur) -> bool {
            if (cur == target) return true;
            if (visited.count(cur)) return false;
            visited.insert(cur);
            auto cell_it = cells_.find(cur);
            if (cell_it != cells_.end() && cell_it->second) {
                for (const auto& dep : cell_it->second->GetOutgoingDependencies()) {
                    if (dfs(dep)) return true;
                }
            }
            return false;
        };
        if (dfs(ref)) return true;
    }
    return false;
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}