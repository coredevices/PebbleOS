class Svg2PdcError(Exception):
    def __str__(self):
        return self.__class__.__name__ + ': ' + ' '.join(self.args)

class Svg2PdcFormatError(Svg2PdcError):
    pass
