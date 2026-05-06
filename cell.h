#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>
#include <optional>

class Sheet;

class Cell : public CellInterface {
public:
    Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    std::unique_ptr<Impl> impl_;
    Sheet& sheet_;
    Position pos_ = Position::NONE;
    mutable std::optional<Value> cached_value_;
    mutable bool dirty_ = true;

    std::vector<Position> outgoing_deps_;
    std::vector<Position> incoming_deps_;

    void UpdateDependencies();
    void ClearDependencies();

    void AddIncomingDependency(Position dependent);
    void RemoveIncomingDependency(Position dependent);

    const std::vector<Position>& GetIncomingDependencies() const;
    const std::vector<Position>& GetOutgoingDependencies() const;

    void InvalidateCache();
    void SetPosition(Position pos);

    friend class Sheet;
};