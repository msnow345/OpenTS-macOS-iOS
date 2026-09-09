// Flattens an icon that carries an alpha channel onto an opaque square canvas. An iOS app
// icon may not carry one, and sips, the only image tool a stock macOS and Xcode host is
// guaranteed to have, cannot composite. Run with
// `swift composite-icon.swift <src> <dst.png> <size> [rrggbb] [inset]`.
//
// `inset` is the fraction of the canvas cropped off each edge by over-drawing the source.
// It exists for artwork that draws its own rounded corners or border, which iOS's icon mask
// would otherwise turn into a double frame.

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

func die(_ message: String) -> Never {
  FileHandle.standardError.write(Data("composite-icon: \(message)\n".utf8))
  exit(1)
}

let args = CommandLine.arguments
guard args.count >= 4 else {
  die("usage: composite-icon.swift <src> <dst.png> <size> [rrggbb] [inset]")
}
let sourcePath = args[1]
let destinationPath = args[2]
guard let size = Int(args[3]), size > 0 else { die("size must be a positive integer") }
let background = args.count > 4 ? args[4] : "111111"
guard background.count == 6, let backgroundValue = UInt32(background, radix: 16) else {
  die("background must be six hex digits, got '\(background)'")
}
let inset = args.count > 5 ? (Double(args[5]) ?? 0) : 0
guard inset >= 0, inset < 0.5 else { die("inset must be in [0, 0.5)") }

guard let sourceData = NSData(contentsOfFile: sourcePath),
      let imageSource = CGImageSourceCreateWithData(sourceData, nil),
      let image = CGImageSourceCreateImageAtIndex(imageSource, 0, nil)
else { die("cannot read image '\(sourcePath)'") }

guard let context = CGContext(
        data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)
else { die("cannot create a \(size)x\(size) bitmap context") }

context.interpolationQuality = .high
context.setFillColor(
  red: CGFloat((backgroundValue >> 16) & 0xff) / 255.0,
  green: CGFloat((backgroundValue >> 8) & 0xff) / 255.0,
  blue: CGFloat(backgroundValue & 0xff) / 255.0,
  alpha: 1)
context.fill(CGRect(x: 0, y: 0, width: size, height: size))

let overdraw = CGFloat(inset) * CGFloat(size)
context.draw(
  image,
  in: CGRect(
    x: -overdraw, y: -overdraw,
    width: CGFloat(size) + 2 * overdraw, height: CGFloat(size) + 2 * overdraw))

guard let flattened = context.makeImage() else { die("cannot snapshot the bitmap context") }
let url = URL(fileURLWithPath: destinationPath) as CFURL
guard let destination = CGImageDestinationCreateWithURL(url, UTType.png.identifier as CFString, 1, nil)
else { die("cannot create '\(destinationPath)'") }
CGImageDestinationAddImage(destination, flattened, nil)
guard CGImageDestinationFinalize(destination) else { die("cannot write '\(destinationPath)'") }
