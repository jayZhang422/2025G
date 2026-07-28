import sys

import pdfplumber


TERMS = (
    "页面管理",
    "新增页面",
    "删除页面",
    "复制页面",
    "导入页面",
    "系统页面",
    "系统键盘",
    "隐藏页",
    "页面名称",
    "编辑器",
)


def main() -> None:
    sys.stdout.reconfigure(encoding="utf-8")
    with pdfplumber.open(sys.argv[1]) as document:
        print(f"PAGES {len(document.pages)}")
        for index, page in enumerate(document.pages, start=1):
            text = page.extract_text() or ""
            lines = text.splitlines()
            matches = []
            for line_index, line in enumerate(lines):
                if any(term.lower() in line.lower() for term in TERMS):
                    start = max(0, line_index - 2)
                    end = min(len(lines), line_index + 5)
                    matches.append("\n".join(lines[start:end]))
            if matches:
                print(f"\n### PAGE {index}")
                print("\n---\n".join(matches))


if __name__ == "__main__":
    main()
