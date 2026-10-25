0 - None: movement simulation without visual representation.
1 - Native ISM: Unreal’s Mass Representation update path.
2 - Niagara: persistent GPU mesh particles reading current and previous transforms from a Niagara Data Channel.
3 - Custom ISM: a custom Mass processor collecting transforms by stable instance index and submitting one bulk ISM update per frame.
