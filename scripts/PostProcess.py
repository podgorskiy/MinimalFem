from PIL import Image, ImageDraw
import argparse
import sys
	
def getNodePosition(i, nodes, deforms):
	(x, y) = nodes[i]
	return (
		x + deforms[2 * i + 0] * 50.0,
		y + deforms[2 * i + 1] * 50.0
	)

def PostProcess(inputfile, outputfile):
	content = []
	with open(inputfile) as f:
		content = f.readlines()

	line = 1
	nodesCount = int(content[line])
	line+=1

	nodes = []

	for i in range(0, nodesCount):
		coordinates = content[line].split()
		line+=1
		nodes.append(
			(
				float(coordinates[0]),
				float(coordinates[1])
			)
		)
		
	(max_x, max_y) = nodes[0]
	(min_x, min_y) = nodes[0]

	for node in nodes:
		(x, y) = node
		if max_x < x:
			max_x = x
		if min_x > x:
			min_x = x
		if max_y < y:
			max_y = y
		if min_y > y:
			min_y = y

	elementsCount = int(content[line])
	line+=1

	elements = []

	for i in range(0, elementsCount):
		elementNodes = content[line].split()
		line+=1
		elements.append(
			(
				int(elementNodes[0]),
				int(elementNodes[1]),
				int(elementNodes[2])
			)
		)
		
	center = [(max_x + min_x) / 2.0, (max_y + min_y) / 2.0]
	scale = max_x - min_x;
	if scale < max_y - min_y:
		scale = max_y - min_y

	deforms = []
	stresses = []
	resultsContent = []
	with open(outputfile) as f:
		resultsContent = f.readlines()

	for line in resultsContent[:nodesCount*2]:
		deforms.append(float(line))
	for line in resultsContent[nodesCount*2:]:
		stresses.append(float(line))
			
	def Transform(point):
		x = (point[0] - center[0]) / scale / 1.5 * image.size[0] + image.size[0]/2.0
		y = (point[1] - center[1]) / scale / 1.5 * image.size[1] + image.size[1]/2.0
		return (x, image.size[1] - y)
		
	image = Image.new("RGB", (1024,1024))

	draw = ImageDraw.Draw(image)
	draw.rectangle([(0, 0), image.size], fill=0x777777)

	for element in elements:
		draw.polygon([Transform(nodes[element[0]]), Transform(nodes[element[1]]), Transform(nodes[element[2]])], fill=0x20a020, outline=0x707020)

	image.save("initial.png", "PNG")

	draw.rectangle([(0, 0), image.size], fill=0x777777)

	for idx,element in enumerate(elements):
		v = stresses[idx] * 0.3
		#float v = floor(w * (u_rangesIntervals.z + 1.0))/u_rangesIntervals.z;
		colorf = ((v - 0.5)*4.0, 2.0 - abs(v- 0.5)*4.0,	2.0 - v*4.0)
		colori = (int(colorf[0]*255), int(colorf[1]*255), int(colorf[2]*255))
		draw.polygon(
			[
				Transform(getNodePosition(element[0], nodes, deforms)),
				Transform(getNodePosition(element[1], nodes, deforms)),
				Transform(getNodePosition(element[2], nodes, deforms))
			], 
			fill = colori
		)

	image.save("deformed.png", "PNG")


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Tool postprocessing output of MinimalFem.',
									 epilog = 'Visit http://podgorskiy.com/spblog/392/writing-a-fem-solver-in-less-the-180-lines-of-code')
	parser.add_argument('inputfile', metavar='inputfile', type=str, help='Path to input file.')
	parser.add_argument('outputfile', metavar='outputfile', type=str, help='Path to output file.')
	args = parser.parse_args()
	PostProcess(args.inputfile, args.outputfile)