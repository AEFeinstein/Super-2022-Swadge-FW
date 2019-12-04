from PIL import Image, ImageSequence
import glob
from pathlib import Path


def isBlackPixel(pix):
    sumChannel = 0
    for channel in pix:
        sumChannel = sumChannel + channel
    return (sumChannel < 382)


def writeBinArray(outFile, array):
    bitIdx = 0
    byteToWrite = 0
    byteArr = bytearray(0)
    for i in range(len(array)):
        byteToWrite = (byteToWrite << 1) | (array[i] & 1)
        bitIdx = bitIdx + 1
        if(bitIdx % 8 == 0):
            # print(hex(byteToWrite), end=' ')
            byteArr.append(byteToWrite)
            bitIdx = 0
            byteToWrite = 0
    outFile.write(byteArr)


def frameToBits(frame):
    # This is saved reading down each column rather than across each
    # row because this image scrolls left/right
    frameBits = []
    pix = frame.load()
    for x in range(frame.width):
        for y in range(frame.height):
            if(isBlackPixel(pix[x, y])):
                frameBits.append(1)
            else:
                frameBits.append(0)
    return frameBits


def diffFrame(lastFrame, currFrame):
    frameDiff = []
    for i in range(len(currFrame)):
        if(lastFrame[i] != currFrame[i]):
            frameDiff.append(1)
        else:
            frameDiff.append(0)
    return frameDiff


def processGifFile(file):
    print("Processing " + file)
    index = 0
    currentFrame = []
    prevFrame = []
    for frame in ImageSequence.Iterator(Image.open(file)):
        print("  Frame " + str(index) + ", duration = " +
              str(frame.info.get("duration")))
        # Get the bits from this image
        currentFrame = frameToBits(frame.convert('RGB'))

        # If this is the first frame
        if len(prevFrame) == 0:
            # write some metadata
            metadata = bytearray()
            metadata.append((frame.width >> 8) & 0xFF)
            metadata.append((frame.width >> 0) & 0xFF)

            metadata.append((frame.height >> 8) & 0xFF)
            metadata.append((frame.height >> 0) & 0xFF)

            metadata.append((frame.n_frames >> 8) & 0xFF)
            metadata.append((frame.n_frames >> 0) & 0xFF)

            duration = frame.info.get("duration")
            if None is duration:
                duration = 0
            elif 0 == duration:
                duration = 100
            metadata.append((duration >> 8) & 0xFF)
            metadata.append((duration >> 0) & 0xFF)

            outFile = open("./bins/" + Path(file).stem +
                           "_" + str(index) + ".bin", "wb+")
            # Print out the first frame and metadata
            outFile.write(metadata)
            writeBinArray(outFile, currentFrame)
            outFile.close
        else:
            # Just print the difference
            outFile = open("./bins/" + Path(file).stem +
                           "_" + str(index) + ".bin", "wb+")
            writeBinArray(outFile, diffFrame(prevFrame, currentFrame))
            outFile.close
        prevFrame = currentFrame
        index = index + 1


if __name__ == "__main__":
    for file in glob.glob('./menu_gifs/*.gif'):
        processGifFile(file)
