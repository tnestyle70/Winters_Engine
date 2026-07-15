package accountpolicy

import (
	"encoding/json"
	"fmt"
	"os"
)

// Policy is the account economy source of truth consumed by the Auth service.
// Values are never duplicated as literals in Services or Client code.
type Policy struct {
	SchemaVersion   int    `json:"schemaVersion"`
	CurrencyCode    string `json:"currencyCode"`
	StartingBalance int64  `json:"startingBalance"`
}

func Load(path string) (*Policy, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read account policy %s: %w", path, err)
	}
	var p Policy
	if err := json.Unmarshal(data, &p); err != nil {
		return nil, fmt.Errorf("parse account policy %s: %w", path, err)
	}
	if p.SchemaVersion != 1 {
		return nil, fmt.Errorf("account policy %s: unsupported schemaVersion %d", path, p.SchemaVersion)
	}
	if p.CurrencyCode == "" {
		return nil, fmt.Errorf("account policy %s: currencyCode is required", path)
	}
	if p.StartingBalance < 0 {
		return nil, fmt.Errorf("account policy %s: startingBalance must be >= 0", path)
	}
	return &p, nil
}
