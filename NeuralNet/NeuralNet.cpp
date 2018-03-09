#include <iostream>
#include <vector>
#include <cstdlib> //for rand() / RAND_MAX
#include <cassert> // for assert()
#include <cmath> //for tanh()
#include <string>//for training data
#include <fstream> //for text file writing
#include <sstream> //stringstream although string may suffice

//Class to read training data from a file
class TrainingData
{
private:
	std::ifstream m_trainingDataFile;
public:
	TrainingData(const std::string filename);
	bool isEof(void) { return m_trainingDataFile.eof(); };
	void getTopology(std::vector<unsigned> &topology);

	//Returns the number of input values read from the file:
	unsigned getNextInputs(std::vector<double> &inputVals);
	unsigned getTargetOutputs(std::vector<double> &targetOutputVals);
};

void TrainingData::getTopology(std::vector<unsigned> &topology)
{
	std::string line;
	std::string label;

	//Create initial nodes
	std::getline(m_trainingDataFile, line);
	std::stringstream ss(line);
	ss >> label;
	if (this->isEof() || label.compare("topology:") != 0)
	{
		abort();
	}

	while (!ss.eof())
	{
		unsigned n;
		ss >> n;
		topology.push_back(n);
	}

	return;
}

TrainingData::TrainingData(const std::string filename)
{
	m_trainingDataFile.open(filename.c_str());
}

unsigned TrainingData::getNextInputs(std::vector<double> &inputVals)
{
	inputVals.clear();

	std::string line;
	std::getline(m_trainingDataFile, line);
	std::stringstream ss(line);
	
	//Develop each cases' values
	std::string label;
	ss >> label;
	if (label.compare("in:") == 0)
	{
		double oneValue;
		while (ss >> oneValue)
			inputVals.push_back(oneValue);
	}

	return inputVals.size();
}

unsigned TrainingData::getTargetOutputs(std::vector<double> &targetOutputVals)
{
	targetOutputVals.clear();
	std::string line;
	std::getline(m_trainingDataFile, line);
	std::stringstream ss(line);

	//Get output Layer
	std::string label;
	ss >> label;
	if (label.compare("out:") == 0)
	{
		double oneValue;
		while (ss >> oneValue)
			targetOutputVals.push_back(oneValue);
	}

	return targetOutputVals.size();
}

struct Connection
{
	double weight;
	double deltaWeight;
};

class Neuron; //needed as forward reference for neuron

typedef std::vector<Neuron> Layer; //can be made private

//-------------------------Neuron----------------------

class Neuron //actual construction of neuron here
{
private:
	static double eta; // {0.0 ... 1} overall net training rate
	static double alpha; // {0.0 ... n} multiplier of last weight change (momentum)
	static double transferFunction(double x);
	static double transferFunctionDerivative(double x); //used for back propogation learning
	static double randomWeight(void) { return rand() / double(RAND_MAX); } // returns a random number between 0 and 1
	double sumDOW(const Layer &nextLayer) const; 
	double m_outputVal;
	std::vector<Connection> m_outputWeights;
	unsigned m_myIndex; //gives each neuron #
	double m_gradient;

public:
	Neuron(unsigned numOutputs, unsigned myIndex);
	void setOutputVal(double val) { m_outputVal = val; } //sets output value to argument
	double getOutputVal(void) const { return m_outputVal; } //the void is not mandatory you can just have empty parameters
	void feedForward(const Layer &prevLayer); //const qualify (aka const isnt necessary but shows nothing changes)
	void calcOutputGradients(double targetVal);
	void calcHiddenGradients(const Layer &nextLayer); //const qualify since we are reading and not overwriting
	void updateInputWeights(Layer &prevLayer);
};

void Neuron::updateInputWeights(Layer &prevLayer)
{
	//the weights to be updated are in the Connection vector in the neurons
	//in the preceding layer

	for (unsigned n = 0; n < prevLayer.size(); ++n) //iterate through previous layer including bias
	{
		Neuron &neuron = prevLayer[n];
		double oldDeltaWeight = neuron.m_outputWeights[m_myIndex].deltaWeight;

		double newDeltaWeight =
			//individual input, magnified by the gradient and train rate
			eta
			* neuron.getOutputVal()
			* m_gradient
			//Also add momentum =  a fraction of the previous delta weight
			+ alpha
			* oldDeltaWeight;

		neuron.m_outputWeights[m_myIndex].deltaWeight = newDeltaWeight; //store new delta weight in neuron
		neuron.m_outputWeights[m_myIndex].weight += newDeltaWeight; // overwrites new weight
	}
}

double Neuron::eta = 0.15; //overall net learning rate, can change to experiment
double Neuron::alpha = 0.5; //momentum, multiplier of last deltaweight {0.0 ... n}

double Neuron::sumDOW(const Layer &nextLayer) const
{
	double sum = 0.0;

	//Sum contributions of the errors at the nodes we feed

	for (unsigned n = 0; n < nextLayer.size() - 1; ++n)
	{
		sum += m_outputWeights[n].weight * nextLayer[n].m_gradient; 
	}

	return sum;
}

void Neuron::calcHiddenGradients(const Layer &nextLayer) //no target value to compare with
{
	double dow = sumDOW(nextLayer); // DOW = derivative of weights
	m_gradient = dow * Neuron::transferFunctionDerivative(m_outputVal); //dow is the delta here
}

void Neuron::calcOutputGradients(double targetVal)
{
	double delta = targetVal - m_outputVal;
	m_gradient = delta * Neuron::transferFunctionDerivative(m_outputVal); 
}

double Neuron::transferFunction(double x)
{
	//tanh - output range {-1.0 ... 1.0}
	return tanh(x); //tanh() is a built in function in c++

}

double Neuron::transferFunctionDerivative(double x)
{
	//derivative of a hyperbolic tangent is 1 - tanh^2(x)
	return 1.0 - (x*x); //approximation
}

void Neuron::feedForward(const Layer &prevLayer)
{
	double sum = 0.0;

	//Sum the previous layer's outputs(which are inputs)
	//Include the bias node from the previous layer

	for (unsigned n = 0; n < prevLayer.size(); ++n) //no -1 since we are including bias neuron
	{
		sum += prevLayer[n].getOutputVal() * prevLayer[n].m_outputWeights[m_myIndex].weight;
	}

	m_outputVal = Neuron::transferFunction(sum); //activation function
}

Neuron::Neuron(unsigned numOutputs, unsigned myIndex) //constructor
{
	for (unsigned c = 0; c < numOutputs; ++c) //c for connections
	{
		m_outputWeights.push_back(Connection());
		m_outputWeights.back().weight = randomWeight(); // can be made into a random inline fucntion, mess around with later
	}

	m_myIndex = myIndex;
}

//---------------------------Net-----------------------

class Net
{
public:
	Net(const std::vector<unsigned> &topology);
	void feedForward(const std::vector<double> &inputVals); //pass by referece to not copy whole vector
	void backProp(const std::vector<double> &targetVals);
	void getResults(std::vector<double> &resultVals, int outMode) const; //the vector is not const because we are writing vals to it
	double getRecentAverageError(void) const { return m_recentAverageError; }; // training data function
private:
	std::vector<Layer> m_layers; //m_layers[layerNum][neuronNum] 2D vector,layer vector of neuron
	double m_error;
	double m_recentAverageError;
	double m_recentAverageSmoothingFactor;
};

void Net::getResults(std::vector<double> &resultVals, int outMode) const
{
	resultVals.clear();
	if (outMode == 1)
	{
		for (unsigned n = 0; n < m_layers.size() - 2; ++n) //-2 to remove bias neuron from output (single output)
		{
			resultVals.push_back(m_layers.back()[n].getOutputVal());
		}
	}
	else if (outMode == 2)
	{
		for (unsigned n = 0; n < m_layers.size(); ++n) //size is the same for multiple outputs
		{
			resultVals.push_back(m_layers.back()[n].getOutputVal());
		}
	}

}


void Net::backProp(const std::vector<double> &targetVals)
{
	//calculate an overall net error (RMS of output neuron errors)

	Layer &outputLayer = m_layers.back(); //easier to read than the .back()
	m_error = 0.0; //new training pass and new error

	for (unsigned n = 0; n < outputLayer.size() - 1; ++n)
	{
		double delta = targetVals[n] - outputLayer[n].getOutputVal(); //delta is the difference between target and output
		m_error += delta * delta; //the square of the sums of the error
	}
	m_error /= outputLayer.size() - 1; //get average of error (-1 for bias)
	m_error = sqrt(m_error); //square root to get RSM

	//implement a recent average measurement (extra but shows performance trend)
	m_recentAverageError = (m_recentAverageError * m_recentAverageSmoothingFactor + m_error) 
		/ (m_recentAverageSmoothingFactor + 1.0);
	
	//calculate output layer gradients
	for (unsigned n = 0; n < outputLayer.size() - 1; ++n)
	{
		outputLayer[n].calcOutputGradients(targetVals[n]);
	}
	//calculate gradients on hidden layers

	for (unsigned layerNum = m_layers.size() - 2; layerNum > 0; --layerNum) //accounts for each hidden layer until none are left
	{
		Layer &hiddenLayer = m_layers[layerNum]; //convenience variable
		Layer &nextLayer = m_layers[layerNum + 1]; //convenience variable

		for (unsigned n = 0; n < hiddenLayer.size(); ++n)
		{
			hiddenLayer[n].calcHiddenGradients(nextLayer);
		}
	}

	//for all layers from outputs to first hidden layer, update connection weights

	for (unsigned layerNum = m_layers.size() - 1; layerNum > 0; --layerNum)
	{
		Layer &layer = m_layers[layerNum];
		Layer &prevLayer = m_layers[layerNum - 1];

		for (unsigned n = 0; n < layer.size() - 1; ++n) //update weights for each enruon
		{
			layer[n].updateInputWeights(prevLayer);
		}
	}
}


//feeds values forward
void Net::feedForward(const std::vector<double> &inputVals)
{
	//error check to make sure input values go to a neuron
	assert(inputVals.size() == m_layers[0].size() - 1); // - 1 for bias neuron

	//Assign (latch) the input values into the input neurons
	for (unsigned i = 0; i < inputVals.size(); ++i)
	{
		m_layers[0][i].setOutputVal(inputVals[i]); // set function via neuron will access input vals to make outputs
	}

	//Forward propagate (loop starts from one to skip inputs)
	for (unsigned layerNum = 1; layerNum < m_layers.size(); ++layerNum)
	{
		Layer &prevLayer = m_layers[layerNum - 1]; //pointer to a previous layer (current layer - 1)
		for (unsigned n = 0; n < m_layers[layerNum].size() - 1; ++n) //address individual numbers to feed forward
		{
			//uses prevLayer to point to neurons in layer prior to obtain output values
			m_layers[layerNum][n].feedForward(prevLayer);// the neuron itself will have a feed forward as well
		}
	}
}

Net::Net(const std::vector<unsigned> &topology) //constructor outside of class for cleanliness
{
	unsigned numLayers = topology.size(); //conveniece
	for (unsigned layerNum = 0; layerNum < numLayers; ++layerNum)
	{
		m_layers.push_back(Layer()); //makes a neural layer 
		
		//if this is the out output layer no outputs needed otherwise set outputs to layer # + 1 for bias
		unsigned numOutputs = layerNum == topology.size() - 1 ? 0 : topology[layerNum + 1]; 

		//made a new layer now to add individual neurons to thatlayer
		for (unsigned neuronNum = 0; neuronNum <= topology[layerNum]; ++neuronNum)
		{
			m_layers.back().push_back(Neuron(numOutputs, neuronNum)); //most recent element in vector
			//std::cout << "Made a Neuron!" << '\n'; not a necessary statement but for debug answer
		}

		//Force the bias node's output to 1.0. It's the last neuron created above
		m_layers.back().back().setOutputVal(1.0);
	}
}

void showVectorVals(std::string label, std::vector<double> &v)
{
	std::cout << label << " ";
	for (unsigned i = 0; i < v.size(); i++)
	{
		std::cout << v[i] << " ";
	}

	std::cout << '\n';
}

void makeXORData() // makes training data
{
	std::ofstream myFile;
	myFile.open("trainingData.txt");
	myFile << "topology: 2 4 1" << '\n';
	for (int i = 5; i > 0; i--)
	{
		int n1 = (int)(2.0 * rand() / double(RAND_MAX));
		int n2 = (int)(2.0 * rand() / double(RAND_MAX));
		int t = n1 ^ n2; //should be 0 or 1 (xor)
		myFile << "in: " << n1 << ".0 " << n2 << ".0 " << '\n';
		myFile << "out: " << t << ".0 " << '\n';
	}

	myFile.close();
}

void makeAndData() 
{
	std::ofstream myFile;
	myFile.open("trainingData.txt");
	myFile << "topology: 2 4 1" << '\n';
	for (int i = 10; i > 0; i--)
	{
		int n1 = (int)(2.0 * rand() / double(RAND_MAX));
		int n2 = (int)(2.0 * rand() / double(RAND_MAX));
		int t = n1 && n2;
		myFile << "in: " << n1 << ".0 " << n2 << ".0 " << '\n';
		myFile << "out: " << t << ".0 " << '\n';
	}

	myFile.close();
}

void makeComplexData()
{
	std::ofstream myFile;
	myFile.open("trainingData.txt");
	myFile << "topology: 3 4 1" << '\n';
	for (int i = 1000; i > 0; i--)
	{
		int n1 = (int)(2.0 * rand() / double(RAND_MAX));
		int n2 = (int)(2.0 * rand() / double(RAND_MAX));
		int n3 = (int)(2.0 * rand() / double(RAND_MAX));
		int t = (n1 && n2) || n3;
		myFile << "in: " << n1 << ".0 " << n2 << ".0 " << n3 << ".0 " << '\n';
		myFile << "out: " << t << ".0 " << '\n';
	}
	
	myFile.close();
}

void testComplexDataMaking()
{
	std::ofstream myFile;
	myFile.open("trainingData.txt");
	myFile << "topology: 6 7 1" << '\n';
	for (int i = 20000; i > 0; i--)
	{
		int n1 = (int)(2.0 * rand() / double(RAND_MAX));
		int n2 = (int)(2.0 * rand() / double(RAND_MAX));
		int n3 = (int)(2.0 * rand() / double(RAND_MAX));
		int n4 = (int)(2.0 * rand() / double(RAND_MAX));
		int n5 = (int)(2.0 * rand() / double(RAND_MAX));
		int n6 = (int)(2.0 * rand() / double(RAND_MAX));
		int t = ((n1 || n2) && n3) && ((n4 && n5) || n6);

		myFile << "in: " << n1 << ".0 " << n2 << ".0 " << n3 << ".0 " << n4 << ".0 " << n5 << ".0 " << n6 << ".0 " << '\n';
		myFile << "out: " << t << ".0 " << '\n';
	}

	myFile.close();
}

void multiOut()
{
	std::ofstream myFile;
	myFile.open("trainingData.txt");
	myFile << "topology: 6 9 3" << '\n';
	for (int i = 20000; i > 0; i--)
	{
		int n1 = (int)(2.0 * rand() / double(RAND_MAX));
		int n2 = (int)(2.0 * rand() / double(RAND_MAX));
		int n3 = (int)(2.0 * rand() / double(RAND_MAX));
		int n4 = (int)(2.0 * rand() / double(RAND_MAX));
		int n5 = (int)(2.0 * rand() / double(RAND_MAX));
		int n6 = (int)(2.0 * rand() / double(RAND_MAX));
		int t1 = (n1 || n2) && n3;
		int t2 = (n4 && n5) || n6;
		int t3 = ((n1 || n2) && n3) && ((n4 && n5) || n6);
		myFile << "in: " << n1 << ".0 " << n2 << ".0 " << n3 << ".0 " << n4 << ".0 " << n5 << ".0 " << n6 << ".0 " << '\n';
		myFile << "out: " << t1 << ".0 " << t2 << ".0 "<< t3 << ".0 " <<'\n';
	}

	myFile.close();
}

int main()
{
	std::cout << "Welcome to the Neural Net\n";
	std::cout << "[Select training: 1) xor, 2) and, 3) Complex, 4) Large Logic Construct 5) Multi-Output Network]: ";
	int oMode = 0;
	int choice;
	do
	{
		std::cin >> choice;
	} while (choice != 1 && choice != 2 && choice != 3 && choice != 4);

	switch (choice)
	{
	case (1):
		makeXORData();
		oMode = 1;
		break;
	case (2):
		makeAndData();
		oMode = 1;
		break;
	case (3):
		makeComplexData();
		oMode = 1;
		break;
	case (4):
		testComplexDataMaking();
		oMode = 1;
		break;
	case (5):
		multiOut();
		oMode = 2;
		break;
	}

	TrainingData trainData("trainingData.txt");
	std::vector<unsigned> topology;
	trainData.getTopology(topology);
	Net myNet(topology);

	std::vector<double> inputVals, targetVals, resultVals;
	int trainingPass = 0;
	while (!trainData.isEof())
	{
		++trainingPass;
		std::cout << '\n' << "Pass: " << trainingPass;

		//Get new input data and feed it forward:
			if(trainData.getNextInputs(inputVals) != topology[0])
			{
				break;
			}
		showVectorVals(": Inputs:", inputVals);
		myNet.feedForward(inputVals);

		//Collect the net's actual results:
		myNet.getResults(resultVals, oMode);
		showVectorVals("Outputs: ", resultVals);

		//Train the net what the outputs should have been:
		trainData.getTargetOutputs(targetVals);
		showVectorVals("Targets: ", targetVals);
		assert(targetVals.size() == topology.back());

		myNet.backProp(targetVals);

		//Report how well the training is working, averaged over recent stages
		std::cout << "Net recent average error: "
			<< myNet.getRecentAverageError() << '\n';
	}

	std::cout << " Done" << '\n';

	system("pause");
	return 0;
}
