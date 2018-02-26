import os
from PIL import Image
from PIL import ImageDraw

# Generates hilbert curve animations based on the specified quantum.
# Will generate frames with several chunks of lines at once to illustrate
# the speedups that SIMD vectorization will allow.
# 															- Wunkolo 2018

def qHilbertSOA(Width, Distances):
	Level = 1
	PositionsX = [0] * len(Distances)
	PositionsY = [0] * len(Distances)
	CurDistances = Distances
	for i in range(Width.bit_length() - 1):
		# Determine Regions
		RegionsX = [0b1 & (Dist // 2)
					for Dist in CurDistances]
		RegionsY = [0b1 & (Dist ^ RegionsX[i])
					for i, Dist in enumerate(CurDistances)]
		# Flip if RegX == 0 RegY==1
		PositionsX = [
			(Level-1 - PosX) if RegionsX[i] == 1 and RegionsY[i] == 0 else PosX
			for i, PosX in enumerate(PositionsX)
		]
		PositionsY = [
			(Level-1 - PosY) if RegionsX[i] == 1 and RegionsY[i] == 0 else PosY
			for i, PosY in enumerate(PositionsY)
		]
		# Swap X and Y if RegX == 0
		PositionsX, PositionsY = zip(
			*[reversed(ele) if RegionsY[i] == 0 else ele
			  for i, ele in enumerate(zip(PositionsX, PositionsY))]
		)
		PositionsX = [PosX + Level * RegionsX[i]
					  for i, PosX in enumerate(PositionsX)]
		PositionsY = [PosY + Level * RegionsY[i]
					  for i, PosY in enumerate(PositionsY)]
		CurDistances = [Dist // 4 for Dist in CurDistances]
		Level *= 2
	return list(zip(PositionsX, PositionsY))

# Config
# Name: Name of generated frames. Default "Hilbert"
# Path: Path for output images Default: "."
# Width: Must be power of 2. Default: 32
# Spacing: Determines the "Width" of the cell (use odd numbers). Default: 3
# Scale: Scaling for the resulting image. Default: 2
# Quantum: Number of lines drawn per-frame. Default: 1


def GenHilbertFrames(Params):

	# Config
	Name = Params.get("Name", "Hilbert")
	Width = Params.get("Width", 32)
	Path = Params.get("Path", ".") + "/"
	Spacing = Params.get("Spacing", 3)
	Scale = Params.get("Scale", 2)
	Quantum = Params.get("Quantum", 1)

	# Create target path recursively
	os.makedirs(Path, exist_ok=True)
	img = Image.new('RGB', (Width*Spacing, Width*Spacing))
	Points = qHilbertSOA(Width, list(range(0, Width**2)))
	draw = ImageDraw.Draw(img)
	SegmentCount = (Width ** 2) // Quantum

	print(
		"%s:\n\tPoints: %u\n\tQuantum: %u\n\tFrames: %u"
		% (Name, len(Points), Quantum, SegmentCount)
	)

	for i in range(0, SegmentCount):

		# Current segment of points
		CurSegment = Points[i * Quantum: i * Quantum + Quantum]
		# Insert end of previous segment, if it exists.
		CurSegment.insert(0, Points[max(i * Quantum - 1, 0)])
		# Scale points within image
		ScaledPoints = [(Point[0] * Spacing, Point[1] * Spacing)
						for Point in CurSegment]
		# Draw Segment
		draw.line(
			ScaledPoints,
			fill=(i, abs((i & 0xFF) - 0x7F), 0xFF - (i & 0xFF))
		)
		# Draw "vertex" of each point
		draw.point(
			ScaledPoints,
			fill=(0xFF, 0x00, 0x00)
		)

		# Print progess
		print(
			"%6d/%6d %02.02f%%\r"
			% (i+1, SegmentCount, 100*((i+1)/SegmentCount)),
			end=''
		)

		# Scale the frame
		img.resize(
			(img.width * Scale, img.height * Scale),
			Image.NEAREST
		).save(Path + Name + str(i).zfill(6) + ".png")
	del draw

# Serial
GenHilbertFrames(
	{
		"Name": "Serial-",
		"Path": "Serial",
		"Quantum": 1,
		"Scale": 6,
	}
)

# SSE 4.2 / NEON
GenHilbertFrames(
	{
		"Name": "SSE42-",
		"Path": "SSE42",
		"Quantum": 4,
		"Scale": 6
	}
)

# AVX2
GenHilbertFrames(
	{
		"Name": "AVX2-",
		"Path": "AVX2",
		"Quantum": 8,
		"Scale": 6
	}
)

# AVX512
GenHilbertFrames(
	{
		"Name": "AVX512-",
		"Path": "AVX512",
		"Quantum": 16,
		"Scale": 6
	}
)

# ffmpeg -f image2 -framerate 50 -i Serial-%6d.png Serial.gif -t 4

