package stats

import (
	"context"
	"fmt"
	"sort"
	"time"

	"github.com/krizdingus/claudometer/daemon/pkg/claudedata"
	"github.com/krizdingus/claudometer/daemon/pkg/routines"
)

type routinesSource interface {
	Get(ctx context.Context) ([]routines.Routine, error)
}

type Aggregator struct {
	// Records is the static set of records used when GetRecords is nil.
	// Tests set this directly; production code should set GetRecords instead
	// so the daemon picks up newly-written JSONL lines between requests.
	Records    []claudedata.Record
	GetRecords func() []claudedata.Record
	PlanInfo   claudedata.PlanInfo
	Caps       claudedata.Caps
	Routines   routinesSource
	Now        func() time.Time
}

func (a *Aggregator) records() []claudedata.Record {
	if a.GetRecords != nil {
		return a.GetRecords()
	}
	return a.Records
}

func (a *Aggregator) Build(ctx context.Context) (Stats, error) {
	now := a.Now()
	caps := a.Caps
	recs := a.records()
	blocks := claudedata.ComputeBlocks(recs, 5*time.Hour)
	active := claudedata.ActiveBlock(blocks, now)

	out := Stats{
		Schema:      SchemaVersion,
		GeneratedAt: now,
		LocalTime:   now.Local().Format("15:04"),
		Session:     buildSession(active, caps, now),
		ModelsToday: buildModelsToday(recs, now),
		Sonnet:      buildSonnet(recs, caps, now),
		Chat:        Chat{MessagesToday: 0, DailyCap: caps.DailyChatMessages, ResetsAt: "00:00"},
		Budgets:     buildBudgets(recs, a.PlanInfo.Plan, caps, now),
	}

	rs, err := a.Routines.Get(ctx)
	if err == nil {
		out.Routines = convertRoutines(rs)
	}
	return out, nil
}

func buildSession(b *claudedata.Block, caps claudedata.Caps, now time.Time) Session {
	if b == nil || caps.SessionBlockTokens == 0 {
		return Session{}
	}
	pct := b.TokensUsed * 100 / caps.SessionBlockTokens
	if pct > 100 {
		pct = 100
	}
	type modelRow struct{ Model string; Tokens int }
	rows := make([]modelRow, 0, len(b.PerModel))
	for m, t := range b.PerModel {
		rows = append(rows, modelRow{m, t})
	}
	sort.Slice(rows, func(i, j int) bool { return rows[i].Tokens > rows[j].Tokens })
	top := []ModelTokenRow{}
	for i, r := range rows {
		if i >= 3 {
			break
		}
		top = append(top, ModelTokenRow{Model: r.Model, Tokens: r.Tokens})
	}
	return Session{
		PctUsed:          pct,
		MinutesRemaining: int(b.End.Sub(now).Minutes()),
		ResetsAt:         b.End.Format("15:04"),
		Models:           top,
	}
}

func buildModelsToday(recs []claudedata.Record, now time.Time) ModelsToday {
	totals := claudedata.TodayUsage(recs, now)
	cost := 0.0
	for _, r := range recs {
		y, m, d := now.Date()
		ry, rm, rd := r.Timestamp.In(now.Location()).Date()
		if y == ry && m == rm && d == rd {
			cost += claudedata.Estimate(r.Model, r.Tokens)
		}
	}
	rows := []ModelTokenRow{}
	for m, t := range totals {
		rows = append(rows, ModelTokenRow{Model: m, Tokens: t})
	}
	sort.Slice(rows, func(i, j int) bool { return rows[i].Tokens > rows[j].Tokens })
	return ModelsToday{TotalTokens: claudedata.Total(totals), ByModel: rows, EstCostUSD: cost}
}

func buildSonnet(recs []claudedata.Record, caps claudedata.Caps, now time.Time) SonnetWeekly {
	weekly := claudedata.WeeklyUsage(recs, now)
	used := 0
	for model, t := range weekly {
		if model == "claude-sonnet-4-6" {
			used += t
		}
	}
	cap := caps.WeeklyAllModels // overall cap for Sonnet on Pro/Max
	pct := 0
	if cap > 0 {
		pct = used * 100 / cap
		if pct > 100 {
			pct = 100
		}
	}
	return SonnetWeekly{WeeklyPct: pct, Used: used, Cap: cap, Pace: "on_track"}
}

func buildBudgets(recs []claudedata.Record, plan claudedata.Plan, caps claudedata.Caps, now time.Time) Budgets {
	weekly := claudedata.WeeklyUsage(recs, now)
	allTotal := claudedata.Total(weekly)
	opus := weekly["claude-opus-4-7"]
	codeAll := 0
	if caps.WeeklyAllModels > 0 {
		codeAll = allTotal * 100 / caps.WeeklyAllModels
		if codeAll > 100 {
			codeAll = 100
		}
	}
	codeOpus := 0
	if caps.WeeklyOpusOnly > 0 {
		codeOpus = opus * 100 / caps.WeeklyOpusOnly
		if codeOpus > 100 {
			codeOpus = 100
		}
	}
	_, end := claudedata.WeeklyWindow(now)
	remaining := end.Sub(now)
	return Budgets{
		CodeAllPct:  codeAll,
		CodeOpusPct: codeOpus,
		ChatPct:     0,
		Plan:        string(plan),
		ResetsIn:    formatDuration(remaining),
	}
}

func convertRoutines(rs []routines.Routine) []Routine {
	out := []Routine{}
	for _, r := range rs {
		last := "—"
		if r.LastRun != nil {
			last = r.LastRun.Local().Format("15:04")
		}
		next := "—"
		if r.NextRun != nil {
			next = r.NextRun.Local().Format("15:04")
		}
		out = append(out, Routine{Name: r.Name, Status: r.LastStatus, LastRun: last, NextRun: next})
	}
	return out
}

func formatDuration(d time.Duration) string {
	if d < 0 {
		return "0h"
	}
	days := int(d.Hours()) / 24
	hours := int(d.Hours()) % 24
	if days > 0 {
		return fmt.Sprintf("%dd%dh", days, hours)
	}
	return fmt.Sprintf("%dh", hours)
}
