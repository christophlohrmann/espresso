from __future__ import print_function, absolute_import
from .script_interface import ScriptInterfaceHelper


class EKBoundaries(ScriptInterfaceHelper):
    _so_name = "LBBoundaries::LBBoundaries"

    def add(self, *args, **kwargs):
        if len(args) == 1:
            if isinstance(args[0], LBBoundary):
                lbboundary = args[0]
            else:
                raise TypeError(
                    "Either a EKBoundary object or key-value pairs for the parameters of a EKBoundary object need to be passed.")
        else:
            lbboundary = LBBoundary(**kwargs)
        self.call_method("add", object=lbboundary)
        return lbboundary

    def remove(self, lbboundary):
        self.call_method("remove", lbboundary=lbboundary)


class EKBoundary(ScriptInterfaceHelper):
    _so_name = "LBBoundaries::LBBoundary"