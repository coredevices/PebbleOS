class PblConvertError(Exception):
    def __str__(self):
        return self.__class__.__name__ + ': ' + ' '.join(self.args)

class PblConvertFormatError(PblConvertError):
    pass
