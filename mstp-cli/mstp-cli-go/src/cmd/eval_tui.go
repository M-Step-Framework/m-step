package cmd

import (
	"fmt"
	"io"
	"strings"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

const listHeight = 14

// ----- styles -----

type evalStyles struct {
	title        lipgloss.Style
	item         lipgloss.Style
	selectedItem lipgloss.Style
	pagination   lipgloss.Style
	help         lipgloss.Style
	quitText     lipgloss.Style
}

func newEvalStyles() evalStyles {
	var s evalStyles
	s.title = lipgloss.NewStyle().MarginLeft(2)
	s.item = lipgloss.NewStyle().PaddingLeft(4)
	s.selectedItem = lipgloss.NewStyle().PaddingLeft(2).Foreground(lipgloss.Color("170"))
	s.pagination = list.DefaultStyles().PaginationStyle.PaddingLeft(4)
	s.help = list.DefaultStyles().HelpStyle.PaddingLeft(4).PaddingBottom(1)
	s.quitText = lipgloss.NewStyle().Margin(1, 0, 2, 4)
	return s
}

// ----- list item (plain string) -----

type evalItem string

func (i evalItem) FilterValue() string { return "" }

// ----- custom delegate -----

type evalDelegate struct {
	styles *evalStyles
}

func (d evalDelegate) Height() int                             { return 1 }
func (d evalDelegate) Spacing() int                            { return 0 }
func (d evalDelegate) Update(_ tea.Msg, _ *list.Model) tea.Cmd { return nil }
func (d evalDelegate) Render(w io.Writer, m list.Model, index int, listItem list.Item) {
	it, ok := listItem.(evalItem)
	if !ok {
		return
	}

	str := fmt.Sprintf("%d. %s", index+1, string(it))

	fn := d.styles.item.Render
	if index == m.Index() {
		fn = func(s ...string) string {
			return d.styles.selectedItem.Render("> " + strings.Join(s, " "))
		}
	}

	fmt.Fprint(w, fn(str))
}

// ----- model -----

type evalModel struct {
	list     list.Model
	chosen   string // "" means not chosen yet
	styles   evalStyles
	quitting bool
}

func (m evalModel) Init() tea.Cmd {
	return nil
}

func (m evalModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.list.SetWidth(msg.Width)
		return m, nil

	case tea.KeyMsg:
		switch keypress := msg.String(); keypress {
		case "q", "esc", "ctrl+c":
			m.quitting = true
			return m, tea.Quit

		case "enter":
			if i, ok := m.list.SelectedItem().(evalItem); ok {
				m.chosen = string(i)
			}
			return m, tea.Quit
		}
	}

	var cmd tea.Cmd
	m.list, cmd = m.list.Update(msg)
	return m, cmd
}

func (m evalModel) View() string {
	if m.chosen != "" {
		return m.styles.quitText.Render(fmt.Sprintf("Running: %s", m.chosen))
	}
	if m.quitting {
		return m.styles.quitText.Render("Evaluation cancelled.")
	}
	return "\n" + m.list.View()
}

// ----- public entry point -----

// runEvalSelector opens an interactive list and returns:
//
//	the selected item's title (e.g. "t1"), or
//	"" if the user cancelled (Esc / q / Ctrl+C).
func runEvalSelector() string {
	items := []list.Item{
		evalItem("t1"),
		evalItem("t2"),
		evalItem("t3"),
		evalItem("t4"),
		evalItem("t5"),
		evalItem("t6"),
		evalItem("t7"),
		evalItem("all"),
	}

	const defaultWidth = 30

	styles := newEvalStyles()
	delegate := evalDelegate{styles: &styles}

	l := list.New(items, delegate, defaultWidth, listHeight)
	l.Title = "Run Evaluation"
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)
	l.Styles.Title = styles.title
	l.Styles.PaginationStyle = styles.pagination
	l.Styles.HelpStyle = styles.help

	m := evalModel{list: l, styles: styles}

	p := tea.NewProgram(m, tea.WithoutCatchPanics())
	finalModel, err := p.Run()
	if err != nil {
		fmt.Printf("eval TUI error: %v\n", err)
		return ""
	}

	fm, _ := finalModel.(evalModel)
	if fm.quitting || fm.chosen == "" {
		return ""
	}
	return fm.chosen
}