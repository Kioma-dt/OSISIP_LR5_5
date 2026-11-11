"""Построение двух диаграмм по результатам экспериментов с TCP-чатом."""
import csv
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
CSV_PATH = ROOT / "data" / "results.csv"
DIAGRAMS = ROOT / "diagrams"
FIXED_MAX_LEN = 256
FIXED_CLIENTS = 3
MESSAGES = 100


def read_runs(path):
    groups = defaultdict(list)
    with path.open(encoding="utf-8-sig", newline="") as file:
        for row in csv.DictReader(file):
            n, length = int(row["clients"]), int(row["max_len"])
            if int(row["count"]) != MESSAGES or (length != FIXED_MAX_LEN and n != FIXED_CLIENTS):
                continue
            settings = tuple(row[k] for k in ("student_id", "seed", "timeout_ms", "host_port"))
            groups[settings, n, length, int(row["run"])].append(row)

    profiles = {key[0] for key in groups}
    if len(profiles) > 1:
        raise ValueError("В CSV разные student_id, seed, timeout_ms или host_port. Выберите одну серию.")

    for (_, n, length, run), rows in sorted(groups.items()):
        names = {row["name"] for row in rows}
        if len(names) != len(rows):
            raise ValueError(f"Повторяются имена: run={run}, clients={n}, max_len={length}. "
                             "Проверьте, не записаны ли два опыта под одним номером.")
        complete = names == {f"u{i}" for i in range(n)}
        success = all(int(row[k]) == MESSAGES for row in rows
                      for k in ("sent", "replies", "ok", "delivered"))
        clean = all(int(row[k]) == 0 for row in rows for k in ("errors", "timeouts"))
        if not (complete and success and clean):
            print(f"Пропущен неполный или ошибочный запуск: run={run}, n={n}, max_len={length}",
                  file=sys.stderr)
            continue
        yield n, length, rows


def draw(groups, title, xlabel, ylabel, filename):
    keys = sorted(groups)
    values = [mean(groups[key]) for key in keys]  # Среднее по повторам конфигурации.
    fig, ax = plt.subplots(figsize=(8, 5), layout="constrained")
    bars = ax.bar([str(key) for key in keys], values, width=0.55, color="#3973a5")
    ax.bar_label(bars, labels=[f"{v:,.2f}".replace(",", " ").replace(".", ",")
                               for v in values], padding=4)
    ax.set(title=title, xlabel=xlabel, ylabel=ylabel, ylim=(0, max(values) * 1.18 or 1))
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    ax.set_axisbelow(True)
    fig.savefig(DIAGRAMS / filename, dpi=200)
    plt.close(fig)


def main():
    messages, received = defaultdict(list), defaultdict(list)
    for n, length, rows in read_runs(CSV_PATH):
        if length == FIXED_MAX_LEN:
            messages[n].append(mean(float(r["send_per_s"]) for r in rows))
        if n == FIXED_CLIENTS:
            received[length].append(mean(float(r["rx_bytes_per_s"]) for r in rows))

    for label, groups in (("Количество клиентов", messages), ("Длина текста", received)):
        if not groups:
            raise ValueError(f"{label}: подходящих успешных запусков нет.")
        for key, runs in sorted(groups.items()):
            print(f"{label} = {key}: учтено запусков — {len(runs)}")

    DIAGRAMS.mkdir(parents=True, exist_ok=True)
    draw(messages, f"Частота подтверждённых сообщений на клиента\n"
                   f"при максимальной длине текста {FIXED_MAX_LEN} байт",
         "Количество клиентов, шт.", "Подтверждённые сообщения, сообщ./с на клиента",
         "messages_vs_clients.png")
    draw(received, f"Скорость получения данных одним клиентом\n"
                   f"при числе клиентов {FIXED_CLIENTS}",
         "Максимальная длина текста сообщения, байт", "Средняя скорость получения, Б/с",
         "receive_rate_vs_max_len.png")


if __name__ == "__main__":
    main()
