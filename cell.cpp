#include "cell.h"
#include "sheet.h"
#include "formula.h"
#include <sstream>
#include <algorithm>

class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual Value GetValue(const SheetInterface& sheet) const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const { return {}; }
};

class Cell::EmptyImpl : public Cell::Impl {
public:
    Value GetValue(const SheetInterface&) const override { return std::string{}; }
    std::string GetText() const override { return {}; }
};

class Cell::TextImpl : public Cell::Impl {
public:
    explicit TextImpl(std::string text) : raw_text_(std::move(text)) {
        if (!raw_text_.empty() && raw_text_[0] == ESCAPE_SIGN) {
            display_text_ = (raw_text_.size() == 1) ? "" : raw_text_.substr(1);
        } else {
            display_text_ = raw_text_;
        }
    }
    Value GetValue(const SheetInterface&) const override { return display_text_; }
    std::string GetText() const override { return raw_text_; }
private:
    std::string raw_text_;
    std::string display_text_;
};

class Cell::FormulaImpl : public Cell::Impl {
public:
    FormulaImpl(std::string text, std::unique_ptr<FormulaInterface> formula)
        : raw_text_(std::move(text)), formula_(std::move(formula)) {
    }
    Value GetValue(const SheetInterface& sheet) const override {
        auto result = formula_->Evaluate(sheet);
        if (std::holds_alternative<double>(result)) return std::get<double>(result);
        return std::get<FormulaError>(result);
    }
    std::string GetText() const override { return "=" + formula_->GetExpression(); }
    std::vector<Position> GetReferencedCells() const override { return formula_->GetReferencedCells(); }
private:
    std::string raw_text_;
    std::unique_ptr<FormulaInterface> formula_;
};

Cell::Cell(Sheet& sheet) 
    : impl_(std::make_unique<EmptyImpl>())
    , sheet_(sheet)
{}

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    if (text.empty()) {
        Clear();
        return;
    }
    if (text[0] == ESCAPE_SIGN) {
        auto new_impl = std::make_unique<TextImpl>(std::move(text));
        ClearDependencies();
        impl_ = std::move(new_impl);
        dirty_ = true;
        sheet_.InvalidateCell(pos_);
        return;
    }
    if (text[0] == FORMULA_SIGN) {
        if (text.size() == 1) {
            auto new_impl = std::make_unique<TextImpl>(std::move(text));
            ClearDependencies();
            impl_ = std::move(new_impl);
            dirty_ = true;
            sheet_.InvalidateCell(pos_);
            return;
        }
        auto formula_ptr = ParseFormula(text.substr(1));
        auto refs = formula_ptr->GetReferencedCells();
        if (sheet_.HasCycleAfterAdding(pos_, refs)) {
            throw CircularDependencyException("Circular dependency detected");
        }
        auto new_impl = std::make_unique<FormulaImpl>(std::move(text), std::move(formula_ptr));
        ClearDependencies();
        impl_ = std::move(new_impl);
        UpdateDependencies();
        dirty_ = true;
        sheet_.InvalidateCell(pos_);
        return;
    }
    auto new_impl = std::make_unique<TextImpl>(std::move(text));
    ClearDependencies();
    impl_ = std::move(new_impl);
    dirty_ = true;
    sheet_.InvalidateCell(pos_);
}

void Cell::Clear() {
    ClearDependencies();
    impl_ = std::make_unique<EmptyImpl>();
    dirty_ = true;
    sheet_.InvalidateCell(pos_);
}

void Cell::SetPosition(Position pos) {
    pos_ = pos;
}

void Cell::InvalidateCache() {
    dirty_ = true;
}

CellInterface::Value Cell::GetValue() const {
    if (dirty_) {
        cached_value_ = impl_->GetValue(sheet_);
        dirty_ = false;
    }
    return *cached_value_;
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return !incoming_deps_.empty();
}

void Cell::AddIncomingDependency(Position dependent) {
    if (std::find(incoming_deps_.begin(), incoming_deps_.end(), dependent) == incoming_deps_.end()) {
        incoming_deps_.push_back(dependent);
    }
}

void Cell::RemoveIncomingDependency(Position dependent) {
    incoming_deps_.erase(std::remove(incoming_deps_.begin(), incoming_deps_.end(), dependent),
                         incoming_deps_.end());
}

const std::vector<Position>& Cell::GetIncomingDependencies() const {
    return incoming_deps_;
}

const std::vector<Position>& Cell::GetOutgoingDependencies() const {
    return outgoing_deps_;
}

void Cell::UpdateDependencies() {
    auto refs = impl_->GetReferencedCells();
    outgoing_deps_ = refs;
    for (const auto& ref : refs) {
        Cell* ref_cell = dynamic_cast<Cell*>(sheet_.GetCell(ref));
        if (ref_cell) {
            ref_cell->AddIncomingDependency(pos_);
        }
    }
}

void Cell::ClearDependencies() {
    for (const auto& ref : outgoing_deps_) {
        Cell* ref_cell = dynamic_cast<Cell*>(sheet_.GetCell(ref));
        if (ref_cell) {
            ref_cell->RemoveIncomingDependency(pos_);
        }
    }
    outgoing_deps_.clear();
}