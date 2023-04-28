#!/bin/env python
from dataclasses import dataclass

# using space to make parsing in c++ easier
CSV_SEP = " "


@dataclass
class Point:
    x: int
    y: int


class Event:
    def __repr__(self) -> str:
        return self.csv_repr()

    def csv_repr(self) -> str:
        return "undefined"

    def c_struct_repr(self) -> str:
        return "undefined"

    def print(self):
        print(self.csv_repr())


@dataclass
class DownEvent(Event):
    time: int
    id: int
    x: int
    y: int

    def csv_repr(self) -> str:
        return CSV_SEP.join(
            ["DOWN"] + list(map(str, [self.time, self.id, self.x, self.y]))
        )

    def c_struct_repr(self) -> str:
        return (
            "wlr_touch_down_event{ nullptr, "
            + f"{self.time}, {self.id}, {self.x}, {self.y}"
            + "}"
        )


@dataclass
class UpEvent(Event):
    time: int
    id: int

    def csv_repr(self) -> str:
        return CSV_SEP.join(["UP"] + list(map(str, [self.time, self.id])))

    def c_struct_repr(self) -> str:
        return "wlr_touch_up_event{ nullptr, " + f"{self.time}, {self.id}" + "}"


@dataclass
class CheckEvent(Event):
    what: str

    def csv_repr(self) -> str:
        return CSV_SEP.join(["CHECK", self.what])


@dataclass
class MoveEvent(Event):
    time: int
    id: int
    x: int
    y: int

    def csv_repr(self) -> str:
        return CSV_SEP.join(
            ["MOVE"] + list(map(str, [self.time, self.id, self.x, self.y]))
        )

    def c_struct_repr(self) -> str:
        return (
            "wlr_touch_motion_event{ nullptr, "
            + f"{self.time}, {self.id}, {self.x}, {self.y}"
            + "}"
        )


def printEvents(events: list[Event]):
    for e in events:
        e.print()


def makeLeftSwipe3(
    start_point: Point,
    distance: int,
    duration: int,
    time_step: int = 10,
    start_time: int = 100,
) -> list[Event]:
    assert duration > time_step

    events = []
    steps = duration // time_step  # might be off by one but w/e
    dist_step = distance // steps

    # NOTE finger0 is start
    finger0 = Point(start_point.x - 50, start_point.y - 10)
    finger1 = Point(start_point.x, start_point.y)
    finger2 = Point(start_point.x + 50, start_point.y - 10)

    events.append(DownEvent(start_time, 0, finger0.x, finger0.y))
    events.append(DownEvent(start_time, 1, finger1.x, finger1.y))
    events.append(DownEvent(start_time, 2, finger2.x, finger2.y))
    events.append(CheckEvent("complete"))

    for t in range(start_time + time_step, start_time + duration, time_step):
        finger0.x -= dist_step
        finger1.x -= dist_step
        finger2.x -= dist_step

        events.append(MoveEvent(t, 0, finger0.x, finger0.y))
        events.append(MoveEvent(t, 1, finger1.x, finger1.y))
        events.append(MoveEvent(t, 2, finger2.x, finger2.y))

    events.append(UpEvent(start_time + duration, 0))
    events.append(UpEvent(start_time + duration, 1))
    events.append(UpEvent(start_time + duration, 2))

    return events


def main():
    printEvents(makeLeftSwipe3(Point(500, 300), 150, 200))


if __name__ == "__main__":
    main()
