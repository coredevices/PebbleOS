class Gif2ApngError(Exception):
    def __str__(self):
        return self.__class__.__name__ + ': ' + ' '.join(self.args)


class Gif2ApngFormatError(Gif2ApngError):
    pass
